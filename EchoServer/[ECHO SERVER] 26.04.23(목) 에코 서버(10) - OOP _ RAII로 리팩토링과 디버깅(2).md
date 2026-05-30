## 0. 서론

오늘은 저번에 이어서 
내 에코 서버의 로직을 OOP / RAII를 적용하는 방식으로 리팩토링을 해봤어.
저번에는 단순히 서버의 객체들을 설계했었고,
이번에는 서버의 main() 함수와 클라이언트의 코드들을 작성했어.

이번에 특히 ListenSockAccept() 함수의 반환값과 복사 금지 충돌 문제를
해결한게 좀 재밌었어.

[깃허브 링크](https://github.com/SiRyus1111/My-First-Echo-Server-Project)

## 1. 전체적인 구조

로직은 그대로 유지되어있어.
코드도 사실 객체 생성하는 부분과 
객체를 기반으로 socket() / bind() / listen() / connect() / accept() / send() / recv() 하는 것,
기본 세팅(listen() / connect()까지)에서는 try-catch로 예외처리를 한다는 것을 제외하고 
사실상 같아.

그저 기존에는 절차적 프로그래밍으로,
그런 함수들이 객체에 묶여있지 않았어서
소켓 함수에 잘못된 소켓을 넣을 수 있다거나,
실수를 해서 closesocket() / WSACleanup() 등을 호출하는 것을 까먹어서
자원 누수가 발생할 수 있었는데,
객체 지향 프로그래밍 / RAII 로 구조를 개편하니
그런 실수를 할 가능성이 상당히 줄어든 것 같아.

그리고, 내가 특히 편했던 부분이 있는데,
기존 send_all() / recv_all() 함수의 인자는 이런 방식으로 들어갔어.
```cpp
int send_all(SOCKET s, NetState& state, const char* msg, int len);
```
`소켓 - 상태 관리 구조체 - 보낼/받을 메시지가 담겨져있는/담길 버퍼 - 받아야하는 데이터의 크기`
그리고 ClientSockSend() / ClientSockRecv() / ConnectSockSend() / ConnectSockRecv() 함수에는
인자가 이런 방식으로 들어갔어.
```cpp
int ClientSockSend(NetState& state, const char* msg, int len);
```
`상태 관리 구조체 - 보낼/받을 메시지가 담겨져있는/담길 버퍼 - 받아야하는 데이터의 크기`
기존 인자 순서에서 맨 앞의 소켓만 제거하면 새 함수들의 인자 순서가 됐어서
기존 함수 -> 새 함수로의 수정이 상당히 간편했어.

이렇게 비슷한 일을 하는, 비슷한 인자들이 필요한 함수들의 인자 순서를 비슷하게 맞춰놓는게
나중에 그런 함수들에 수정이 필요할 때 정말 좋은 구조인 것 같더라.

## 2. ClientSocket의 복사 방지와 ListenSockAccept()의 반환 값 문제 해결

이게 사실상 이번의 메인인데,
기존의 ClientSocket 객체는
```cpp
ClientSocket(const ClientSocket&) = delete;
ClientSocket& operator=(const ClientSocket&) = delete;
```
이렇게 생성자에 의한 복사나 대입에 의한 복사를 완전히 막아놓았어서
```cpp
ClientSocket client_sock = listen_sock.ListenSockAccept(&client_addr);
```
이렇게 ListenSocket 객체로 accept()함으로써 생성된 소켓을 대입 연산자(`=`)로 
따로 ClientSocket 객체로 전달하는데에 문제가 있었어.

물론 C++17 이상이라면 이런 것은 컴파일러가 자동으로 이동으로 판별해서
대입(복사)가 아닌 이동으로 처리를 해주지만,
C++17 이하의 환경에서 실행할 때는 문제가 될 수 있었어.

그래서 나는 따로 이동 생성자라는 것으로 이 문제를 해결했어.
```cpp
// 이동 생성자
ClientSocket(ClientSocket&& other) noexcept : client_sock(other.client_sock) { // noexcept = 이 함수는 예외를 발생시키지 않는다고 컴파일러에게 알려주기. 그래서 이동 최적화.
    other.client_sock = INVALID_SOCKET;
}
```
여기서 `ClientSocket&& other`는 r-value reference(r-value 참조 변수)야.

r-value를 아주 거칠게 말하면,
"이름으로 다시 접근할 수 없는 값" 또는 "곧 사라질 수 있는 값"에 가까워.

예를 들어,

```cpp
int a = 2 + 3;
```
에서 2 + 3은 계산 결과로 만들어지는 값이지만,
이 값 자체에 이름이 붙어 있지는 않아.

즉 a는 이후에도 이름으로 접근할 수 있지만,
2 + 3의 결과값(5)은 그 표현식이 끝나면 더 이상 직접 접근할 수 없지.

이런 값이 r-value라고 생각하면 돼.


내 코드에서는 이 부분이 중요했어.

```cpp
return ClientSocket(client_sock);
```
여기서 ClientSocket(client_sock)은 이름 없는 임시 ClientSocket 객체인데,

이 객체는 함수의 반환값으로 사용되고,
그 이후에는 원래 위치(ListenSocket 객체)에서 계속 사용할 이유가 없어.

즉, 이 객체가 들고 있는 SOCKET 핸들을
새로운 ClientSocket 객체로 넘겨도 되는거야.

이때 그 임시 객체를 받을 수 있는 매개변수 타입이
```cpp
ClientSocket&& other
```
이야.

즉 ClientSocket&&는
> 이 객체는 곧 사라질 수 있는 임시 객체이므로,
그 내부 자원을 복사하지 않고 옮겨올 수 있다
는 의미를 표현하는 참조 타입

라고 보면 돼.

다만, 주의점이 있는데, 
`ClientSocket&& other`라고 해서 `other` 자체가 계속 r-value처럼 행동하는 것은 아니야.

이동 생성자 내부에서 `other`는 이름이 있는 변수이기 때문에,
표현식으로 사용할 때는 l-value처럼 취급돼.

하지만 결국 `other`가 참조하고 있는 원본은
이동 생성자에 전달된 임시 객체야.

그래서 이동 생성자 안에서는
`other.client_sock`이 가지고 있던 소켓 핸들을 새 객체로 가져오고,
기존 객체는 더 이상 그 소켓을 닫지 못하도록 무효화하는거야.
```cpp
other.client_sock = INVALID_SOCKET;
```
이렇게 무효화를 해줘서

복사를 막아놓은 이유인
double close 문제도 해결할 수 있게 했어.

딱 해당 소켓 핸들의 소유권, 해당 자원의 소유권(ownership)만
새 객체로 이동시키는거야.

### 2-1. 왜 ClientSocket은 복사를 막아야 했을까?

ClientSocket 객체는 소켓 핸들을 소유하는 객체야.

즉, ClientSocket 객체 하나 = 소켓 핸들 하나
나 다름이 없지.

하지만 복사가 허용된다면,
두 객체가 같은 소켓 핸들을 공유하게 돼.

이 때, 두 객체가 동시에 스코프를 벗어나며 두 객체의 소멸자에서 각각 closesocket()이 호출된다면,
방금 말했던 double close 문제가 발생할 수 있어.

그래서 ClientSocket을 포함한 다른 소켓 객체들은

```cpp
ClientSocket(const ClientSocket&) = delete;
ClientSocket& operator=(const ClientSocket&) = delete;
```
로 복사를 막는거야.

### 2-2. noexcept를 사용한 이유

그리고 이렇게 
```cpp
// 이동 생성자
ClientSocket(ClientSocket&& other) noexcept : client_sock(other.client_sock) { // noexcept = 이 함수는 예외를 발생시키지 않는다고 컴파일러에게 알려주기. 그래서 이동 최적화.
    other.client_sock = INVALID_SOCKET;
}
```
noexcept 키워드로 이동 생성자가 예외를 발생시키지 않는다고 컴파일러에게 명시해줘서
컴파일러가 더 적극적으로 이동 최적화를 사용할 수 있게 했어.

그리고 ClientSocket 이동 생성자는
- 단순한 값 복사
- INVALID_SOCKET 대입

이 두 가지만 수행하므로
이 두 가지의 과정에서 예외가 발생할 가능성이
사실상 없는 수준이라고 생각해서 이렇게 noexcept를 붙였어.

## 3. OOP / RAII 적용 전과 후의 차이

기존 코드에서는 이런 순서로 자원을 직접 관리해야했어.

서버

1. WSAStartup() - 윈속 초기화
2. socket() - listen용 소켓 생성
3. bind() - listen용 소켓에 IP:Port 바인딩
4. listen() - listen용 소켓을 LISTEN 상태로 전환
5. accept() - listen용 소켓으로 들어온 연결 정보로 클라이언트와 통신할 소켓 생성
6. recv() / send() - 클리이언트와 통신할 소켓으로 송수신
7. closesocket() - 클라이언트와 통신할 소켓을 닫기
8. closesocket() - listen용 소켓을 닫기
9. WSACleanup() - 윈속 종료

클라이언트

1. WSAStartup() - 윈속 초기화
2. socket() - 서버와 통신할 소켓 생성
3. connect() - 서버의 IP:Port로 연결 요청, 3-way handshake 진행
4. send() / recv() - 서버와 통신할 소켓으로 송수신
5. closesocket() - 서버와 통신할 소켓을 닫기
6. WSACleanup() - 윈속 종료

이런 함수들,
특히 소켓과 관련된 자원들을 해제하는 함수들인 
closesocket() 과 WSACleanup()을 직접 호출해야만 했어.

하지만 중간에 예외가 발생하거나
중간에 return이 실행되면
이런 자원을 해제하는 함수들이 호출되지 않을 가능성이 충분히 있었어.

하지만 RAII 구조로 변경한 이후에는

- WinsockGuard - 윈속 초기화와 종료를 담당할 객체
- ListenSocket - 서버에서 listen을 할 용도의 소켓을 주체로 만들어진 객체
- ClientSocket - 서버에서 accept()로 생성된 클라이언트와 통신할 소켓을 주체로 만들어진 객체
- ConnectSocket - 클라이언트에서 서버와 통신할 소켓을 주체로 만들어진 객체

이런 객체들에 윈속 초기화 / 종료와 각각의 소켓을 묶어서
소멸자가 호출될 때 해당하는 자원을 자동으로 정리하게 만들었기 때문에,
예외가 발생하거나 함수가 중간에 종료되더라도
자원 누수가 발생하지 않도록 만들 수 있었어.