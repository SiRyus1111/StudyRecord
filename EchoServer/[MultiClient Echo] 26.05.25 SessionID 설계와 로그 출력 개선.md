## 0. 서론

저번 글에서, 개선 사항으로 
- ClientSession들을 구별하는 SessionID 도입
- 로그 출력 형식을 통일하고 적절한 곳에 출력하도록 로그 출력 개선

이 나왔었지.

해당 개선 사항들을 실제로 반영해봤어.

솔직히 SessionID 시스템은 금방 끝났지만,
로그 출력 개선에서 뇌절을 해부렸어ㅋㅋ 산으로 가버렸다고 할 수 있겠지..

요약하자면(요약이 아니긴 한데),
- SessionID
    - 코드에서는 ClientManger 객체가 각 클라이언트 세션을 식별하기 위한 식별자(identifier)
    - 출력에서는 로그에 각 클라이언트 세션의 ID를 출력해서 로그를 읽는 사람이 세션을 식별하기 위한 식별자
    - `unordered_map`의 `key-value` 구조로 `RemoveClient()`할 때 
    `ClientSession`을 `shared_ptr` 대조로 느리게(`O(N)`) 찾지 않고, 
    `SessionID`이라는 `key`를 기준으로 `shared_ptr<ClientSession>`라는 `value`을 
    빠르게(`O(1)`) 찾을 수 있게 개선.
    - `INITIAL_SESSION_ID`부터 `SessionID`를 선형적으로 증가하는 식으로 부여해서
    SessionID의 중복을 피함
    - 오버플로우가 발생할 경우를 차단하기 위해서 `uint64_t` 형을 `SessionID`로 사용. 
- 로그 출력 개선
    - 공유 자원인 `std::cout`에 여러 스레드가 동시에 접근하여 로그 한 줄의 의미가 깨지는 상황을 막기 위해 전역 로그 출력 전용 객체인 `LineLogger` 도입
      - `LineLogger` 객체에서 `std::cout`에 대한 동기화 수행
        - `std::mutex`를 사용해서 한 번에 한 스레드만 `std::cout`로 로그를 출력할 수 있도록 설계
        - 모든 로그 출력이 같은 `std::mutex`를 공유해야하므로 싱글톤 패턴을 적용해 전역에 딱 한 개의 `LineLogger` 객체만 존재할 수 있게 설정함
      - 한 줄의 로그를 하나의 `std::string`으로 `std::cout`를 사용해서 출력해서 `<<`연산자를 최대한 적게 사용함으로써 출력 시의 자원 소모 최소화
      - 각 계층의 로그 출력 편의 함수 제공(현재는 `WriteSessionLog()` 함수)
    - 클라이언트 연결 / 연결 종료 시에 해당 클라이언트를 식별하는 로그 출력 추가
    - 각 로그의 출력 위치 설정
      - `CONNECTED` : 클라이언트 연결됨
      (accept() 직후, 실제로는 ClientSession의 생성자에서 출력)
      - `RECEIVING` : 수신 중
        (recv_all() 내부에서 recv() 함수를 호출한 직후마다, 현재 미사용)
      - `RECV_COMPLETE` : 수신 완료
      (ClientSession::RecvPacket() 내부)
      - `SENDING` : 송신 중
      (send_all() 내부에서 send() 함수를 호출한 직후마다, 현재 미사용)
      - `SEND_COMPLETE` : 송신 완료
      (ClientSession::SendPacket() 내부)
      -`DISCONNECTED` : 정상 종료
      (TransportExceptionHandling() 함수 내부의 정상 종료 부분에서)
      - `PROTOCOL_ERROR` : 사용자 정의 애플리케이션 레벨 프로토콜 에러
        (이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
      - `TRANSPORT_ERROR` : L4 에러, 송수신 중 에러, 즉 SOCKET_ERROR가 반환되었을 때
      (이것도 TransportExceptionHandling() 함수 내부의 Transport Error 처리 부분에서)
      - `RECEIVE_ERROR_PACKET` : header.type == PacketType::HEADER_ERROR인 패킷을 받은 경우
      (이것도 TransportExceptionHandling() 함수 내부의 Peer Error 처리 부분에서)
      - `SEND_ERROR_PACKET` : header.type == PacketType::HEADER_ERROR인 패킷을 송신하는 경우
      (이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
      
## 1. SessionID 시스템 도입

### 1-1. SessionID 시스템의 필요성

기존의 `ClientManager`의 `std::vector` `clients` 컨테이너 기반 세션 관리는 문제가 좀 있었어.
바로, 
> 클라이언트 세션을 식별할 수 있는 고유한 식별자(identifier)가 없다는 것

이야.

아무리 `std::vector`의 인덱스를 기준으로 `ClientSession`들을 식별한다고 해도,
확실한 해결책은 되지 못했어.
일단 제일 큰 문제였던건
> 딱히 `ClientManager`의 컨테이너 선에서 `ClientSession`들을 확실히 구분하지 못하니
특정 `ClientSession`에 대한 작업을 수행하는데 자원이 좀 든다.
(기존에는 `shared_ptr`을 비교하는 식으로 특정 `ClientSession`을 탐색함)

> 추후 다른 객체나 스레드가 `ClientManager`의 `clients` 컨테이너를 거치지 않고
`ClientSession`들을 다룰 때 
특정 `ClientSession`을 구분할 수단이 없다.

였어.

그래서, 필요했던게

> 어느 객체에서라도 확실히 각 `ClientSession`을 구분할 식별자(identifier)

가 필요했어.

그래서 각 `ClientSession`에 고유한 식별자인 `SessionID`이라는 시스템이 필요했어.

### 1-2. `SessionID`의 설계 조건과 실제 설계

`SessionID`에게 필요했던 조건들은,

1. 하나의 `SessionID`는 정확히 하나의 `ClientSession`만을 식별해야한다.
    - 논리식으로 표현하면,
	$$
    \forall i \in SessionID,\ \exists! s \in ClientSession \text{ such that } ID(s)=i
    $$
2. 모든 `ClientSession`은 정확히 하나의 `SessionID`만을 가져야 한다.
    - 논리식으로 표현하면
    $$
	\forall s \in ClientSession,\ \exists! i \in SessionID \text{ such that } 	ID(s)=i
	$$
    
결론은, 
> `ClientSession`과  `SessionID`는 **일대일 대응**이어야 한다.

라는거야.

컴퓨터과학적, 프로그래밍적 관점에서 이 조건을 만족하려면,

1. `SessionID`를 담당하는 변수가 오버플로우가 발생하면 안된다.
    - 아이디의 고유성이 깨짐
2. 각 `SessionID`가 단 한 번씩만 부여되는 로직이 필요하다.
    - 위의 조건과 얽혀서, 아이디의 고유성이 깨짐

이 결론에 도달하게 돼.

결국 `ClientSession`은 복사가 불가능한 고유의 객체이니,
`SessionID`를 잘 부여해야겠지..

그래서 내가 설계한 `SessionID`는,

1. `SessionID`의 자료형은 `uint64_t`를 사용한다.
    - $UMax_{64} = 2^{64} - 1 =  18,446,744,073,709,551,615$ 
    - 1초에 1억번의 아이디 부여가 발생해도 5800년 동안 오버플로우가 발생하지 않는다.
    - 즉, 사실상 오버플로우가 발생하지 않는다.
2. 부여되는 `SessionID`는 선형으로 증가한다.
    - 선형으로 증가하고, 오버플로우가 발생하지 않으니 사실상 각 `SessionID`는 한 번씩만 부여된다.
    - 첫 `SessionID`, 즉 `INITIAL_SESSION_ID`를 상수로 놓고, 
    해당 `INITIAL_SESSION_ID`를 기반으로 계속 `next_session_id`를 1씩 상승시킨다.
      - `accept()`가 실행된 직후 `ClientSession`을 만들 때 
      생성자에서 `next_session_id`에 맞춰 `SessionID`를 부여한다.
      - `SessionID` 부여가 끝나면 다음 `ClientSession`을 위해 
      `next_session_id`를 1 증가시킨다.
3. `SessionID` 부여 후 1 증가 과정에서 레이스 컨디션 발생 우려가 있으나,
현재 구조에서는 오직 한 스레드(`main` 스레드)에서만 
`accept()` 후 `SessionID` 부여 및 증가를 맡고 있으니 레이스 컨디션을 구조적으로 발생하지 않는다.
   - 추후 여러 스레드에서 `accept()` 하는 구조가 되었을 때는 
   `atomic`이나 `mutex` 등의 동기화 도구가 필요하다.
   
이 정도가 되겠네..

### 1-3. 장점

이 구조의 장점은, 그냥 앞에서 말했던 기존 구조의 단점을 해결할 수 있다는거?

> `ClientManager::clients`에서 
`vector` 대신 `unordered_map`을 사용해서 
`key-value`구조로 각 `ClientSession`들을 확실히 구분해서
특정 `ClientSession`을 `SessionID`로 빠르게 찾을 수 있다.

> 추후 다른 객체나 스레드가 `ClientManager::clients`를 거치지 않고
`ClientSession`을 다룰 때 확실한 식별자가 되어준다.

> 로그를 찍을 때도 각 `ClientSession`을 확실히 식별할 수 있다.

이정도.

## 2. 로그 출력 개선

### 2-1. `std::cout` 자체의 문제

`std::cout`는 출력 스트림인데,
이게 **공유 자원**인지라,
여기서 원자성이 보장이 되지 않는다면
여러 스레드가 동시에 `std::cout`를 사용할 때
레이스 컨디션이 발생할 수 있었어.

그리고 만약 콘솔 출력에서 레이스 컨디션이 발생하게 된다면
로그 한 줄의 의미가 깨질 수 있었지.

그래서 아예 `std::cout`를 한 번에 한 스레드만,
로그 한 줄을 기준으로 `std::mutex`를 사용해 동기화하는 구조가 필요했어.

### 2-2. `LineLogger` 클래스

전역에서, 모든 타 라이브러리들에서 `std::cout`를 대체해서 사용되기 때문에,
어느 코드에서나 이식성이 좋은 `LineLogger.h`로 별도의 라이브러리로 분리했어.

```cpp
#pragma once

#include <iostream>
#include <utility>
#include <sstream> // ostringstream 사용하기 위해 인클루드 / ostringstream은 숫자나 다른 타입의 변수들을 문자열 형태로 결합할 때 씀.
#include <stdint.h>
#include <mutex>
#include <string>

class LineLogger{
private:
    std::mutex output_mutex_;
    LineLogger() = default; // 싱글톤 패턴을 위해 생성자를 숨김(private)
public:

    enum class LogType {
        CONNECTED, // 클라이언트 연결됨(accept() 직후, 실제로는 ClientSession의 생성자에서 출력)
        RECEIVING, // 수신 중(recv_all() 내부에서 recv() 함수를 호출한 직후마다)
        RECV_COMPLETE, // 수신 완료(ClientSession::RecvPacket() 내부 or ClientSocket::ClientSockRecv() 함수 내부)(후자가 더 자연스럽긴 함..)
        SENDING, // 송신 중(send_all() 내부에서 send() 함수를 호출한 직후마다)
        SEND_COMPLETE, // 송신 완료(ClientSession::SendPacket() 내부 or ClientSocket::ClientSockSend() 함수 내부)(이것도 후자가 더 자연스럽긴 함..)
        DISCONNECTED, // 정상 종료(TransportExceptionHandling() 함수 내부의 정상 종료 부분에서)
        PROTOCOL_ERROR, // 사용자 정의 애플리케이션 레벨 프로토콜 에러(이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
        TRANSPORT_ERROR, // L4 에러, 송수신 중 에러, 즉 SOCKET_ERROR가 반환되었을 때(이것도 TransportExceptionHandling() 함수 내부의 Transport Error 처리 부분에서)
        RECEIVE_ERROR_PACKET, // header.type == PacketType::HEADER_ERROR인 패킷을 받은 경우(이것도 TransportExceptionHandling() 함수 내부의 Peer Error 처리 부분에서)
        SEND_ERROR_PACKET // header.type == PacketType::HEADER_ERROR인 패킷을 송신하는 경우(이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
    };

    static const char* LogTypeToCstyleString(LogType l) {
        switch (l) {
            case LogType::CONNECTED: return "CONNECTED";
            case LogType::RECEIVING: return "RECEIVING";
            case LogType::RECV_COMPLETE: return "RECV_COMPLETE";
            case LogType::SENDING: return "SENDING";
            case LogType::SEND_COMPLETE: return "SEND_COMPLETE";
            case LogType::DISCONNECTED: return "DISCONNECTED";
            case LogType::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
            case LogType::TRANSPORT_ERROR: return "TRANSPORT_ERROR";
            case LogType::RECEIVE_ERROR_PACKET: return "RECEIVE_ERROR_PACKET";
            case LogType::SEND_ERROR_PACKET: return "SEND_ERROR_PACKET";
            default: return "UNKNOWN_LOGTYPE";
        }
    }

    // 복사 생성자 및 대입 연산자, 이동 생성자 까지 전부 차단
    LineLogger& operator=(const LineLogger&) = delete;
    LineLogger(const LineLogger&) = delete;
    LineLogger& operator=(LineLogger&&) = delete;
    LineLogger(LineLogger&&) = delete;

    // 정적 메서드로 유일한 LineLogger 객체를 반환해서 싱글톤 패턴 완성
    // 항상 동일한 객체의 참조를 반환함.
    static LineLogger& GetInstance() {
        static LineLogger instance; // 최초 호출 시 한 번만 객체가 생성됨.
        return instance;
    }
    

    template <typename... Args>
    void WriteLog(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        oss << '\n';

        std::lock_guard<std::mutex> lock(output_mutex_); // 락을 잡는 시간을 최소화해서 락 경합 최소화 및 데드락 가능성 감소
        std::cout << oss.str();
    }

    // [SessionID : ID][IP:Port][LogType] Message
    template <typename... Args>
    void WriteSessionLog(
        uint64_t sessionId, 
        const char* ipaddr, 
        uint16_t port, 
        LogType logType, 
        Args&&... args) {
    
        WriteLog("[SessionID ", sessionId, "]",
                "[", ipaddr, ":", port, "]",
                "[", LogTypeToCstyleString(logType), "] ",
                std::forward<Args>(args)...);
    }

};
```

### 2-3. 코드 동작 요약

`WriteLog()` 함수

1. 가변 인자 템플릿으로 한 줄에 출력할 인자들을 자료형 상관없이 입력받는다.
2. 해당 인자들을 `std::ostringstream`과 폴드 표현식(fold expression)으로 
하나의 `std::string` 문자열로 묶어준다.
3. `output_mutex_`를 획득하고 `std::cout`로 합쳐진 `std::string` 문자열을 출력한다.
4. 문자열 출력 후 락을 해제한다. 

`WriteSessionLog()` 함수

1. 가변 인자 템플릿으로 세션ID, IP주소, 포트번호, 로그타입, 메시지를 입력받는다.
2. 로그 형식에 맞춰서 `WriteLog()` 함수에 추가적인 인자들을 덧붙여 넘겨준다.

`LogType` `enum class`

1. `LineLogger` 클래스의 `LogType`으로 지정될 수 있는 상수들이 모여있다.

`LogTypeToCstyleString()`

1. `LogType` `enum class`의 멤버를 `char*` C언어 스타일 문자열로 변경한다.

`output_mutex_`

1. `std::cout` 출력 전에 각 스레드가 획득해야하는 뮤텍스

### 2-4. 설계 의도 요약

설계 의도들에 대해 설명해보자면,

1. 여러 스레드가 동시에 `std::cout`를 사용할 때, 
로그 한 줄의 의미를 보호하기 위해 `std::mutex`와 `std::lock_guard`를 사용해서
각 스레드가 `output_mutex_`를 잡고 있어야 `std::cout`를 사용할 수 있게 한다.
   - 로그 한 줄의 원소들을 하나의 문자열로 조립하는 과정은 다른 스레드와 공유되지 않으니
   `output_mutex_`를 잡지 않는다.
   - 완성된 문자열, 즉 로그 한 줄을 출력할 때는 다른 스레드와 공유하는 자원인
   `std::cout`를 사용하게 되므로, `output_mutex_`로 로그 한 줄의 의미가 깨지지 않게
   `std::cout`를 보호한다.
   - `std::cout`를 사용할 때만 짧게 락을 잡음으로써, 
   데드락 위험을 낮추고 ,
   많이 수행되는 **로그 출력**이라는 동작을 수행하는데 락이 필요할 때에 
   필연적으로 발생할 수 밖에 없는
   락 경합(lock contention)을 최소화한다.

2. 모든 출력에서 같은 `output_mutex_`를 공유해야 `std::cout`에 대한 원자성이 보장되므로
전역에 하나의 `LineLogger` 객체만 존재해야한다.
   - 그러므로 생성자를 `private` 접근 제어자로 설정해서 일반적인 객체 생성을 막고,
`static` 메서드로 객체 외부에서 유일한 `LineLogger` 객체만 참조할 수 있게 하는 식으로
싱글톤 패턴을 적용했다.
   - 이렇게 싱글톤 패턴을 사용하면, 전역에서 해당 객체에 접근할 때, 
`LineLogger`는 오직 한 객체만 존재하기 때문에,
`std::cout`을 사용할 때 오로지 하나의 `output_mutex_`라는 락을 사용하게 되어서
각 `std::cout`의 원자성이 보장된다.

3. `enum class`로 로그의 타입들을 모아놓음으로써
`WriteSessionLog()` 함수 등 계층 별 로그 출력 인터페이스 함수들의 로그 타입을 입력할 때
프로그래머가 잘못된 로그 타입을 입력하는 실수를 상당히 줄일 수 있다.

4. 하나의 `std::string` 문자열로 만들어서 출력함으로써
기존의 `std::cout`에 `<<`연산자 여러번으로 출력하던 구조에서
단 한번의 `<<`연산자로 처리해서 출력에 쓰이던 자원을 줄일 수 있다.
   - 이 설계는 `std::cout`의 사용 시간을 줄여서,
   여러번의 `<<`연산자로 처리하는 것보다
   락을 잡았을 때의 데드락 위험과 락 경합이 줄어든다.

### 2-5. 어디에 어떤 `LogType`의 로그를 출력해야할까?

이것도 상당히 중요해.
미리 정해놓고 실제 코드를 작성해야
의도를 크게 벗어나지 않는 선에서 코드를 작성할 수 있겠지.

- `CONNECTED` : 클라이언트 연결됨
      (제일 권장되는 위치는 accept() 직후, 
      하지만 accept() 직후에는 `SessionID`를 확실히 알 수 없기 때문에, 
      실제로는 ClientSession의 생성자에서 출력)
- `RECEIVING` : 수신 중
        (recv_all() 내부에서 recv() 함수를 호출한 직후마다, 
        `recv()` 한 번당 로그를 출력하면 쓸데없는 로그가 많이 출력될 수 있으므로
        현재 미사용)
- `RECV_COMPLETE` : 수신 완료
      (ClientSession::RecvPacket() 내부, 
      오로지 정상적으로 수신을 완료한 경우만, 예외 발생시 해당 로그 대신 예외 로그 출력)
- `SENDING` : 송신 중
      (send_all() 내부에서 send() 함수를 호출한 직후마다,
      `send()` 한 번당 로그를 출력하면 쓸데없는 로그가 많이 출력될 수 있으므로
      현재 미사용)
- `SEND_COMPLETE` : 송신 완료
      (ClientSession::SendPacket() 내부,
      오로지 정상적으로 송신을 완료한 경우만, 예외 발생시 해당 로그 대신 예외 로그 출력)
- `DISCONNECTED` : 정상 종료
      (TransportExceptionHandling() 함수 내부의 정상 종료 부분에서 사용.)
- `PROTOCOL_ERROR` : 사용자 정의 애플리케이션 레벨 프로토콜 에러
       (이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
- `TRANSPORT_ERROR` : L4 에러, 송수신 중 에러, 즉 SOCKET_ERROR가 반환되었을 때
      (이것도 TransportExceptionHandling() 함수 내부의 Transport Error 처리 부분에서)
- `RECEIVE_ERROR_PACKET` : header.type == PacketType::HEADER_ERROR인 패킷을 받은 경우
      (이것도 TransportExceptionHandling() 함수 내부의 Peer Error 처리 부분에서)
- `SEND_ERROR_PACKET` : header.type == PacketType::HEADER_ERROR인 패킷을 송신하는 경우
      (이것도 TransportExceptionHandling() 함수 내부의 Protocol Error 처리 부분에서)
      
실제로도 코드에 이렇게 반영 되어있어.

### 2-6. 장점

`LineLogger`를 도입하면서 로그 출력 방식이 하나의 객체를 중심으로 통일되었어.
그래서 이 구조의 장점은,

1. 로그 출력의 일관성 확보

기존에는 각 코드 위치에서 직접 `std::cout`을 사용해 로그를 출력했는데,
이 방식은 로그 형식이 코드 위치마다 달라질 수 있고,
나중에 로그 형식을 수정하려면 여러 위치를 직접 수정해야 한다는 문제가 있었어.

하지만 `LineLogger`를 사용하면 로그 출력 형식을 딱 한 객체에서 관리할 수 있어.
예를 들어 현재 세션 로그는 다음과 같은 형식으로 출력돼.

```text
[SessionID 1][127.0.0.1:50000][CONNECTED] Client connected
```

이처럼 로그 형식이 통일되면,
로그를 읽을 때 각 로그가 어떤 세션에서 발생했는지,
어떤 종류의 이벤트인지 빠르게 파악할 수 있겠지.

그리고 이걸 복사 붙여넣기 해서 로그를 분석할 때
파싱하기도 편해지고.

2. 멀티스레드 환경에서 로그 한 줄의 의미 보존

멀티스레드 환경에서는 여러 스레드가 동시에 `std::cout`라는 공유 자원에 접근할 수 있기 때문에,
각 스레드가 직접 `std::cout`을 사용하면
서로 다른 로그 메시지가 한 줄 안에서 섞일 수 있어.

`LineLogger`는 로그 한 줄을 먼저 하나의 문자열로 조립한 뒤,
`output_mutex_`를 잡은 상태에서 `std::cout`에 출력한다.

따라서 `LineLogger`를 통해 출력되는 로그는
로그 한 줄 단위의 의미가 깨지지 않고 출력돼.

3. 출력 책임의 중앙화

로그 출력 로직을 각 클래스나 함수에 흩어놓지 않고
`LineLogger`라는 하나의 객체에 모아서,
각 계층의 코드는 직접 `std::cout`을 다루지 않고,
`WriteLog()` 또는 `WriteSessionLog()` 같은 인터페이스를 통해 로그를 출력할 수 있어.

이렇게 하면 로그 출력 방식이 바뀌더라도
전체 코드를 수정하지 않고 `LineLogger` 내부 구현만 수정하면 되겠지.

4. 확장 가능성 확보

현재는 로그를 콘솔에 출력하지만,
나중에는 파일 로그, 로그 레벨, 타임스탬프, 스레드 ID 출력 등을 추가할 수도 있겠지?

이때 모든 코드가 직접 `std::cout`을 사용하고 있다면
출력 방식을 바꾸기 어렵다.

하지만 로그 출력 경로가 `LineLogger`로 모여 있으면,
출력 대상이나 로그 형식을 확장하기 훨씬 쉽다.

그리고 아예 다른 계층(예 : Transport Layer)의 로그 출력도 따로 인터페이스 함수를 만들어주는 식으로
편하게 추가할 수도 있고.

5. 로그 타입 실수 감소

`LogType`을 `enum class`로 정의했기 때문에,
문자열을 직접 입력하는 방식보다 잘못된 로그 타입을 사용할 가능성이 확연히 줄어들어.

예를 들어 `"CONNECTD"`처럼 오타가 난 문자열을 직접 넣는 방식보다,
`LogType::CONNECTED`처럼 정해진 값만 사용하도록 만드는 방식이 훨씬 안전하지.

## Ex. `ClientSession::TransportExceptionHandling()` 함수의 개편

### Ex-1. 별개의 `NetState` 구조체를 값 복사로 받음

기존 `ClientSession::TransportExceptionHandling()` 함수,
즉, 세션 종료 / 예외 감지 후 후처리 함수는
아무런 인자도 받지 않았어.

```cpp
void TransportExceptionHandling();
```

그리고 `ClientSession`의 `ClientState`에 전적으로 의존해서
종료 / 예외 감지 후 후처리를 했지.

하지만 이 구조는, 나중에 브로드캐스트가 추가되어서
다른 `ClientSession`에 의해
해당 `ClientSession`이 `SendPacket()` / `RecvPacket()` 등의 `ClientState`를 변경하는 함수들이
호출되는 경우에,

`ClientState`에 저장되어있던 기존 종료 / 예외 상황이
새로운 종료 / 예외 상황으로 갱신된 채
후처리가 진행될 수 있었어.

그래서, 어떤 함수 실행 후 발생한 종료 / 예외 상황을 후처리하는지에 대한
명시가 필요했어.
그래서
```cpp
void TransportExceptionHandling(NetState state);
```
이렇게 `SendPacket()` / `RecvPacket()`을 호출한 결과를 나타내는 `NetState`를 따로 복사로 받아서,
어떤 종료 / 예외 상황을 후처리하는지를 명확히 했어.

이 개편의 의도는, 
> `TransportExceptionHandling()`함수의 `ClientState` 고정 의존을 줄이고,
예외처리 흐름을 더 명확하게 만든다.

라고 할 수 있어.

### Ex-2. if_header_error = true일 때의 패킷 재전송 개편

기존의 `NetState::if_header == true`일 때,
즉, 상대가 유효하지 않은 헤더를 보냈을 때에
상대에게 에러 패킷 재전송하는 로직은
그냥 따로 `ClientSock->ClientSockSend()`이런 식으로
헤더 따로, 페이로드 따로 보냈지만,

현재는 패킷 송 / 수신이 `SendPacket()` / `RecvPacket()`으로 통일됐기 때문에,
개편이 필요했어.

그래서 에러 패킷을 따로따로 보내는 것이 아닌 
`SendPacket()` 함수의 `PacketType type`을 `PacketType::HEADER_ERROR`로 명시해서
하나의 `SendPacket()` 함수로 보내는 식으로 개편했어.

그리고 기존에는 해당 에러 패킷 송신 시의 에러를 
그냥 `std::cout`로 실패 메시지만 콘솔에 출력하고 
그대로 계속 `TransportExceptionHandling()` 함수를 이어갔었는데,

로그 출력 방식(`LineLogger`)과 로그 타입이 명확화되었기 때문에,
새로운 로그 타입(`SEND_ERROR_PACKET_SEND_ERROR`?)을 추가하던지,
아니면 한번 더 `TransportExceptionHandling()`을 호출하던지 해야됐어.

근데 솔직히 전자는 로그가 너무 지저분해질 것 같았고,
후자는 무한 재귀에 빠지지 않을까 걱정했어.

하지만 자세히 살펴보니,
후자가 무한 재귀에 빠질 가능성은 없었어.

1. 후자가 무한 재귀에 빠지는 경우는 
`SendPacket()` 호출 후 반환한 `NetState`가 
`NetState::if_header_error == true`인 경우밖에 없다.
2. `SendPacket()`에서 `if_header_error == true`가 되는 경우는
`SendPacket()`에 넣은 페이로드 길이가 
$(length \leq 0) \lor (length > \text{PAYLOAD SIZE(4096)})$거나,
정의되지 않은 `PacketType`를 `SendPacket()`에 넣은 경우밖에 없다.

논증해보면,
- 전제 1 : `SendPacket()`을 호출 후 `if_header_error`가 `true`라면 오류가 발생한다.
- 전제 2 : `SendPacket()`에서 `if_header_error`가 `true`인 경우는 
페이로드 길이가 잘못되었거나, 정의되지 않은 패킷 타입을 인자로 전달한 경우이다.
- 전제 3 : `SendPacket()`에 에러 메시지와 에러 타입을 넣는 경우에는
페이로드 길이가 잘못되었거나, 정의되지 않은 패킷 타입들 인자로 전달하는 경우가 없다.

논리식으로 표현해보면,
- $Packet(p)$ : p는 SendPacket()에 전달된 패킷이다.
- $HeaderError(p)$ :
`SendPacket(p)`의 결과로 `if_header_error == true`
- $InvalidLength(p)$ :
payload length가 유효 범위를 벗어남
- $InvalidType(p)$ :
PacketType이 정의되지 않음
- $ErrorPacket(p)$ :
HEADER_ERROR 패킷 송신에 사용되는 패킷


- 전제 2 : 
$\forall p (HeaderError(p) \to (InvalidLength(p) \lor InvalidType(p)))$
- 전제 2의 대우 : 
$\forall p (\neg (InvalidLength(p) \lor InvalidType(p)) \to \neg HeaderError(p))$
- 전제 3 : 
$\forall p (ErrorPacket(p) \to (\neg InvalidLength(p) \land ¬InvalidType(p)))$
- 전제 3에 드 모르간의 법칙 적용 : 
$\forall p (ErrorPacket(p) \to \neg (InvalidLength(p) \lor InvalidType(p)))$


$\forall p ( \neg (InvalidLength(p) \lor InvalidType(p)) \to \neg HeaderError(p))$
$\forall p (ErrorPacket(p) \to \neg (InvalidLength(p) \lor InvalidType(p)))$
$\therefore ErrorPacket(p) \to \neg HeaderError(p)$(삼단논법)

> 결론 : `HEADER_ERROR` 패킷 송신 결과에서
`if_header_error`가 다시 `true`가 되는 경우는 존재하지 않는다.

요약 : 

에러 패킷 송신 후 다시 `TransportExceptionHandling()`을 호출해도
무한 재귀가 발생하지 않는다.

왜냐하면 그 후속 호출은 `RecvPacket()` 결과가 아니라
`SendPacket()` 결과를 처리하는 호출이기 때문이다.

현재 `SendPacket()`에 전달하는 에러 패킷은
`PacketType::HEADER_ERROR`와 고정된 에러 메시지의 페이로드를 사용한다.

따라서 페이로드 길이(length)는 유효 범위 안에 있고,
`PacketType`도 정의된 값이다.

그러므로 이 `SendPacket()` 결과에서
다시 `if_header_error`가 `true`가 될 가능성은 없다.

즉, 후속 `TransportExceptionHandling()` 호출에서는
`transport error`나 `eer exit` 같은 송신 과정의 예외만 처리하게 되며,
다시 `header error` 처리 분기로 들어가 무한 재귀하는 구조가 아니다.

이 정도로 정리할 수 있겠네.