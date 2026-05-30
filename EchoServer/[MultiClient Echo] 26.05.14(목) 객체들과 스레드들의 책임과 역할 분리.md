## 0. 서론

이제 사실상 멀티클라이언트 에코서버의 기반이 되는 설계는 이게 마지막이야.

이번 글에서는
지금까지 진행했던 설계들의 총 결산과 같은 느낌의,
> 객체들과 스레드들의 책임 / 역할 분리

에 대해서 설명해볼게.

책임 / 역할을 분리해야할 객체 / 스레드는,

- `ClientManager` 객체
- `ClientSession` 객체
- `main` 스레드
- `client_thread` 스레드

이렇게 네 가지야.

다른 객체(`ListenSocket` / `ClientSocket` / `WinsockGuard` 등)들은,
- 해당 객체가 소유하고있는 자원 / 객체
- 해당 객체의 역할

들이 전부 다 설계가 멀티클라이언트 구조를 적용하기 전에 끝나있던 객체들이라서..

어쨌든 방금 저렇게 네 가지의 객체 / 스레드들에 대한 책임 / 역할을 설명해볼게.

진짜 내가 실시간으로 설계했던건 레포지토리의
`IdeaNote/객체와 스레드들의 역할, 책임(1).png`보면 됨.
[깃허브 링크](https://github.com/SiRyus1111/MultiThreaded-Echo-Chat-Server)

그리고 추가적으로 전체적인 객체의 흐름과
내가 생각한 브로드캐스트 흐름도 설명해볼게. 이건 부록 느낌이야.

### 0-1. 왜 책임과 역할을 각 객체 / 스레드 별로 분리하는가

이걸 꼭 설명해야할 것 같아서 서론에 세부 챕터를 뒀어.

왜 그런지 세 줄(아님) 요약하자면,

1. 각 객체 / 스레드별로 미리 역할을 정해둬야 다른 객체 / 스레드의 역할을 침범하지 않는다.
    - 미리 설계를 정해놓는 것이라고 할 수 있지?
    그림 그릴 때도 미리 스케치를 그려서 형태를 잡고 세부묘사를 하잖아?
    그런 느낌..
2. 코드를 작성할 때 미리 정한 객체 / 스레드들의 역할을 보고 빠르게 코드를 작성할 수 있다.
    - 이런걸 미리 안 정해놓으면 1번과 합쳐져서 
    코드 짤 때 정말 머리아프고 헷갈리고,
    코드는 더러운 코드가 될 것 같았어.
3. 멀티스레드 환경에서 어떤 스레드가 어떤 객체를 건드리는지 미리 정해놔야 동기화 버그 / 객체 수명 문제를 판별하기 쉽다.
    - 제일 어렵지만 어쩌면 제일 중요한거..
    각 객체 / 스레드의 다른 객체 / 스레드에 대한 의존성을 확실히 정리해놔야해.
    그래야 멀티스레드 환경에서의 디버깅이 쉬워.

## 1. Class `ClientManager`

> ClientSession들을 관리하는 클래스

- `ClientSession`들의 목록인 `clients` 관리
  - `ClientSession`들을 `shared_ptr`로 소유
- 관리 목록(`clients`)에 `ClientSession` 추가(`AddClient()`) / 제거(`RemoveClient()`)
- 브로드캐스트 구현 시, `ClientSession`들에 각 `ClientSession` 객체의 `SendPacket()` 실행(아직 미구현)
- 추후 `ClientSession`을 각 `ClientSession`의`SessionID`로 식별 예정

```cpp
class ClientManager : public std::enable_shared_from_this<ClientManager> {
private:
	std::vector<std::shared_ptr<ClientSession>> clients; // ClientSession들을 관리하는 관리 목록, shared_ptr로 소유
	std::mutex client_mutex; // clients 컨테이너에 접근할 때 데이터 레이스를 방지하기 위한 mutex
public:
	// clients에 입력받은 ClientSession을 추가하는 함수, clients에 접근할 때 락 필요
	void AddClient(std::shared_ptr<ClientSession> client); 

	// clients에 입력받은 ClientSession을 제거하는 함수, clients에 접근할 때 락 필요
	void RemoveClient(std::shared_ptr<ClientSession> client); 

	// ClientManager 객체 복사 방지용
	ClientManager& operator=(const ClientManager& c) = delete;
	ClientManager(const ClientManager&) = delete;
};
```

## 2. Class `ClientSession`

> 하나의 클라이언트와 관련된 정보를 저장하고 동작을 명시하는 클래스

- 하나의 클라이언트에 대한 `ClientSocket`을 `unique_ptr`로 소유
- 해당 클라이언트의 주소를 저장
- `ClientSocket` 객체의 `ClientSockSend()` / `ClientSockRecv()`를 이용한 송 / 수신
  - `SendPacket()` - 그저 구현만
  - `RecvPacket()` - 그저 구현만
  - 현재는 에코 서버이므로 `Run()` 함수로 송 / 수신을 전부 담당 가능
  - 추후 함수 분리 예정
- `client_thread()` 스레드의 상태 기록 및 처리
  - 송수신 상태와 송수신 과정의 예외 상황을 기록하고, 
  스레드 종료 후 상태를 반환할 때 사용 = `NetState` 구조체
    - header / payload 송수신 진행 상태 기록
    - `transport error` / `peer exit` / `protocol error` 등의 통신 예외 상태 기록
    - `NetState` 구조체에는 추후 다른 예외 상황도 추가 예정
  - 예외가 발생한 경우 해당 예외에 맞는 처리 수행 
    - `transport error` = 연결 자체의 신뢰성을 보장할 수 없으므로 그냥 종료
    - `peer exit` = 해당 클라이언트가 연결을 종료했으므로 굳이 메시지를 보낼 필요가 없기 때문에 그냥 종료
    - `protocol error` = Application Layer의 프로토콜 오류 때문에 더이상 통신을 하기 어렵지만,
    연결 자체는 신뢰성이 있는 상태이므로 에러 메시지를 송신 후 종료
  - 스레드 실행 중 예외 상황(논리적 종료 상태 등)을 기록할 때는 `std::atomic` 등을 이용해서 
  데이터 레이스 없이 즉시 기록
- 브로드캐스트 구현 시, `SendPacket()` 함수를 사용할 때에 
해당 `ClientSession`의 `SendPacket()` 전용 락 소유
- `ClientManager`에 접근할 때 사용할 `weak_ptr`
  - `ClientManager`에 접근할 때 해당 객체의 수명에 영향을 주지 않기 위해서
  - `ClientManager`과의 순환 참조를 방지하기 위해서
  - `ClientManager`에서 `AddClient()` 함수를 실행해서 
  `clients`에 해당 `ClientSession`을 추가할 때 
  `ClientSession`의 `weak_ptr`도 초기화

```cpp
// 완성된 구조
class ClientSession : public std::enable_shared_from_this<ClientSession> {
private:
	std::unique_ptr<ClientSocket> ClientSock; // 하나의 클라이언트에 대한 ClientSocket을 unique_ptr로 소유
    sockaddr_in ClientAddr; // 해당 클라이언트의 주소를 저장할 소켓 주소 구조체
    std::weak_ptr<ClientManager> Manager_wp; // ClientManager에 접근할 때 사용할 weak_ptr
    NetState ClientState; // 클라이언트의 현재 상태를 기록
    std::atomic<bool> closing = false; // 논리적 종료 상태 기록
    
    // 추후 브로드캐스트 구현 시 추가 예정
    // std::mutex send_mutex;
    
public:
	// 생성자로 ClientSock / ClientAddr / ClientState 초기화
    ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) 
    	: ClientSock(std::move(s)), 
    	ClientAddr(addr), 
    	ClientState{} {
   	 
    } 
    
    // 송 / 수신을 담당하는 함수
    void Run();
    
    // ClientManager::AddClient()에서 호출되는 함수
	// ClientManager의 shared_ptr을 전달받아 Manager_wp를 초기화함.
    void AddToManager(std::shared_ptr<ClientManager> Manager_sp);
    
    // Manager_wp로 ClientManager 객체의 소멸 여부 확인 후 
    // ClientManager 객체의 RemoveClient() 함수를 호출하는 함수
    void RemoveThisClient(); 
    
    // ClientSession 외부에서 closing 변수의 값을 확인하는데 사용하는 함수
    bool IsClosing() const; 
    
    // closing = true로 수정하는 함수
    void MarkClosing();
    
    // ClientSession 객체 복사 방지용
	ClientSession& operator=(const ClientSession& c) = delete;
	ClientSession(const ClientSession&) = delete;
};
```

## 3. thread `main()`

> 서버의 기본적 구조를 담당하고,
새로운 클라이언트 연결을 수락 후 
해당 연결을 `ClientSession` + `client_thread` 구조로 넘기는 스레드

1. `WinsockGuard` 생성
2. `ListenSocket` 생성
3. `ClientManager` 생성
4. `accept()` 루프 실행
4-1. 새 클라이언트 연결 수락
4-2. `ClientSocket` 생성
4-3. `ClientSession` 생성
4-4. `ClientManager`에 `ClientSession` 등록
4-5. `ClientSession`을 이용해서 `client_thread` 생성
4-6. 생성한 `std::thread` 객체와 스레드를 `detach()`
5. 서버 종료 조건 관리

```
// 완성된 main() 스레드의 흐름
main thread
  ├── WinsockGuard 생성
  ├── ListenSocket 생성
  ├── ClientManager 생성
  ├── accept() loop
  │     ├── ClientSocket 생성
  │     ├── ClientSession 생성
  │     ├── ClientManager::AddClient(session)
  │     ├── std::thread ClientThread(client_thread, session)
  │     └── ClientThread.detach()
  └── accept() loop 계속 진행
  └── 서버의 종료 조건 관리
```


## 4. thread `client_thread()`

> 하나의 `ClientSession`의 실행 흐름을 담당하는 스레드

- 특정 `ClientSession` 한 개를 담당
  - `shared_ptr<ClientSession>`을 인자로 받음.
  - 해당 객체가 스레드 종료 전에 소멸되면 안되기 때문에 
  `ClientSession`의 생존을 보장해주기 위해서 `shared_ptr`을 사용.
  - 단독 소유 아님, `ClientManager`과 공동 소유에 가까움
- 실제 송 / 수신 루프 담당
  - 즉, 실제 `ClientSession`객체의 송 / 수신 함수가 호출됨
    - 현재 = `Run()`
    - 송 / 수신 함수 분리 후 = `SendPacket()` / `RecvPacket()`
  - 하지만 송 / 수신 과정에서의 예외처리는 `ClientSession` 객체의 함수들이 담당
    - `NetState` 구조체에 기록
    - `closing` `atomic` 변수에 기록
    - 전부 `ClientSession` 객체의 함수들이 담당(캡슐화)
- 필요시 `std::promise` / `std::future`로 해당 `client_thread`의 종료 결과 전달

```
// 완성된 client_thread() 스레드의 흐름
client_thread
  └── session->Run()
          ├── header recv
          ├── payload recv
          ├── echo send
          ├── error / peer_exit / protocol error 감지
          ├── closing = true
          ├── RemoveThisClient()
          └── return;
```

브로드캐스트가 적용된 후에는,
```
// 브로드캐스트가 적용된 client_thread() 스레드의 흐름
client_thread
  └── recv/send loop
  │        session->RecvPacket()
  │           ├── packet recv
  │           ├── error / peer_exit / protocol error 감지
  │           │     ├── closing = true
  │           │     ├── RemoveThisClient()
  │           │     └── break
  │         session->RequestBroadcast()
  │           ├── ClientManager::Broadcast()
  │           ├── error / peer_exit / protocol error 감지
  │           │     ├── closing = true
  │           │     ├── RemoveThisClient()
  │           │     └── break
  │           └── recv/send loop 계속 진행
  └── return
```

## 5. 전체적인 흐름 정리


main thread
  - accept()
  - ClientSocket 생성
  - ClientSession 생성
  - ClientManager에 등록
  - client_thread 생성 / detach

client_thread
  - ClientSession::Run()
  - 해당 클라이언트와 송수신
  - 종료 상황 발생 시 closing = true
  - ClientManager에 RemoveClient 요청

ClientManager
  - ClientSession 목록 관리
  - AddClient() / RemoveClient()
  - 추후 Broadcast() 담당

ClientSession
  - ClientSocket 소유
  - NetState / closing 관리
  - 자기 클라이언트와의 송수신 담당

각 객체를 한 문장 + 비유로 정리하면
> main() 스레드는 연결을 받는,
입구에서 손님을 받고 자리로 안내해주는 
안내원

> ClientManager는 세션 목록을 관리하는,
즉 전체적인 손님들의 정보를 관리하는 
홀 매니저

> ClientSession은 클라이언트 한 개의 상태와 송 / 수신을 담당하는,
즉 손님의 정보가 기록되어있고
음식을 손님에게 전달할 방법이 적혀있는
손님 한 명의 테이블 정보 / 주문서 / 연락 수단

> client_thread는 그 ClientSession들의 실제 실행 흐름을 담당하는
즉 실제로 손님에게서 주문을 받고, 
손님에게 음식을 전달하는
담당 종업원

---

## EX. 브로드캐스트 아이디어


$1$. 브로드캐스트 전체에 하나의 락을 거는 방식
- Broadcast() 시작 시 락
- 모든 ClientSession send 완료 후 락 해제

문제점:
- 한 클라이언트에게 send 중일 때
다른 클라이언트 송신까지 전부 막힘
- 병렬성이 너무 낮음
- 느린 클라이언트 하나가 전체 브로드캐스트를 지연시킬 수 있음

---

$2$. ClientSession별 send 락 분리 방식 (현재 아이디어)

핵심 발상:
보호해야 하는 것은 "브로드캐스트 전체"가 아니라,
"같은 ClientSession에 대한 패킷 송신 순서"이다.

즉,
같은 클라이언트에 대해:
`[Header][Payload]`
를 보내는 도중 다른 send가 끼어들지만 않으면 됨.

```
// 이렇게 되면 안됨
Header_A
Header_B
Payload_A
Payload_B
```

```
// 이렇게 되기만 하면 됨
Header_A
Payload_A
Header_B
Payload_B
```

그래서:
- 각 ClientSession마다 send mutex를 따로 둔다.
- SendPacket() 진입 시 해당 ClientSession의 send mutex lock
- header + payload 전체 전송 후 unlock

예시(pseudo code에 가까움):
```cpp
class ClientSession {
private:
    std::mutex send_mutex;

public:
    void SendPacket(Packet packet) {
        std::lock_guard<std::mutex> lock(send_mutex);

        send(header); 
        send(payload);
    }
};
```

그리고 Broadcast()에서는:
- ClientManager의 clients_mutex로
클라이언트 목록만 안전하게 접근
- 실제 send는 각 ClientSession 내부 mutex에 맡김

결과:
- 서로 다른 ClientSession의 send는 병렬 진행 가능
- 같은 ClientSession의 send만 직렬화됨
- 패킷 섞임 방지 가능
- 기존 방식보다 브로드캐스트 병목 감소