## 0. 서론

이전까지 계속 C++의 OOP(객체 지향 프로그래밍)과 RAII에 대해 공부하고 있었는데,
기존 에코 서버 프로젝트는 단순히 절차적 프로그래밍 방식으로 코딩되어있던게 기억났어.

그래서 객체 지향 프로그래밍을 좀 더 체화해본다는 생각으로,
기존의 에코 서버를 객체 지향 프로그래밍과 RAII의 방식으로 리팩토링해보기로 결정했어.

과정에서 버그도 많았고, 기상천외한 해결 방법들도 많았어..

이번 글에서는

1. 기존 에코 서버에서의 변경점
1-1. 기존의 함수 / 구조체들을 사용자 정의 헤더로 분리
1-2. 객체 지향 프로그래밍으로 리팩토링한 클래스들
2. 리팩토링 과정에서의 버그들과 디버깅

이렇게 설명해볼게.

그리고 아직 main() 함수 내부는 딱히 건들지 않았어.

## 1. 기존 에코 서버에서의 변경점

지금까지 만들어놓은 에코 서버는 OOP / RAII가 적용되어있지 않았고,
다양한 구조체 / 함수들 때문에 코드가 좀 더러웠어.

나는 그래서

1. 기존의 main() 함수 밖의 함수 / 구조체를 싹다 NetCommon.h라는 헤더 파일에 넣었고
2. 기존의 기능들을 윈속 / listen용 소켓 / 클라이언트와 통신할 소켓으로
WinsockGuard / ListenSocket / ClientSocket클래스로
전부 밀어넣었어.

일단 이 두 가지 변경점에 대해서 설명해볼게.

### 1-1. 기존의 함수 / 구조체들을 사용자 정의 헤더로 분리

```cpp
#pragma once

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Common.h"
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

/*
enum class PacketType : int32_t {
	HEADER_ERROR = 0,
	SAFE = -1
};
*/

#pragma pack(push, 1)
struct PacketHeader {
	int32_t type;
	uint32_t length;
};
#pragma pack(pop)

const int32_t HEADER_ERROR = 0;
const int32_t SAFE = -1;


struct NetState {
	// 진행
	bool header_recv = false;
	bool payload_recv = false;
	bool header_send = false;
	bool payload_send = false;

	// 예외
	bool if_error = false;
	bool if_peer_exit = false;
	bool if_header_error = false;
	bool if_peer_error = false;
};

// 헤더 규칙
// 첫 4바이트 = int32_t 패킷 타입
// 다음 4바이트 = uint32_t 페이로드 길이
// 만약 패킷 타입의 값이 SERVER_HEADER_ERROR(0)이라면 protocol(Application Layer) error.
// 만약 패킷 타입의 값이 SAFE(-1)이라면 일반적인 메시지.

inline int send_all(SOCKET sock, NetState& state, const char* msg, int len) {

	int sent_byte = 0;

	while (sent_byte < len) {
		int send_len = send(sock, msg + sent_byte, len - sent_byte, 0);

		if (send_len == SOCKET_ERROR) {
			err_display("send()");
			state.if_error = true;
			return SOCKET_ERROR;
		}

		sent_byte += send_len;

		if (sent_byte != len) {
			std::cout << "부분적 송신 : " << send_len << '/' << len << "바이트 송신됨.\n";
		}
	}

	std::cout << "송신 완료 : 총 " << sent_byte << '/' << len << "바이트 송신됨\n";

	return sent_byte;
}

inline int recv_all(SOCKET sock, NetState& state, char* buf, int len) {

	int received_byte = 0;

	while (received_byte < len)
	{
		int recv_len = recv(sock, buf + received_byte, len - received_byte, 0);

		if (recv_len == SOCKET_ERROR) {
			err_display("recv()");
			state.if_error = true;
			return SOCKET_ERROR;
		}
		else if (recv_len == 0) {
			state.if_peer_exit = true;
			return 0;
		}

		received_byte += recv_len;

		if (received_byte != len) {
			std::cout << "부분적 수신 : " << recv_len << '/' << len << "바이트 수신됨.\n";
		}
	}


	std::cout << "수신 완료 : 총 " << received_byte << '/' << len << "바이트 수신됨.\n";

	return received_byte;
}
```

이렇게, 기존의 
- PacketHeader 구조체 
- flags(NetState) 구조체
- send_all() 함수
- recv_all() 함수

를 별개의 사용자 정의 헤더로 분리했어.

분리한 이유는, 
- 이 구조체들과 함수들은 
다음 프로젝트인 멀티 클라이언트 에코 서버에서도 쓰일 구조체 / 함수들이고
- 기존의 싱글 클라이언트 에코 서버 코드에서 상당히 큰 부분을 차지하면서 
코드 가독성에 상당히 영향을 주던 함수들이다보니까

별개의 사용자 정의 헤더로 분리하게 됐어.

살짝의 변경점은, send_all() / recv_all()에 inline 키워드를 새로 적어놓은건데,
프로그램 전체에서 여러번 정의되더라도 링크 오류를 발생시키지 않게 하기 위해서
따로 inline 키워드로 두 함수를 정의했어.

### 1-2. 객체 지향 프로그래밍으로 리팩토링한 클래스들

저번의 배웠던 C++의 OOP(Object Oriented Programming)과 RAII를 
에코 서버에도 적용한 것들인데, 
총 세 가지 클래스로 만들었어.

- winsock을 담당하는 WinsockGuard 클래스
- 서버의 listen용 소켓을 담당하는 ListenSocket 클래스
- 서버의 클라이언트와 통신하는 소켓을 담당하는 ClientSocket 클래스

이렇게 자원에 따라 세 가지로 클래스를 만들었어.

### 1-2-1. WinsockGuard

```cpp
class WinsockGuard {
public:
	WinsockGuard() {
		WSADATA wsa;
		int WSAStartupres = WSAStartup(MAKEWORD(2, 2), &wsa);
		if (WSAStartupres != 0) {
			std::cerr << "에러 코드 : " << WSAStartupres << '\n';
			throw std::runtime_error("윈속 초기화 실패");
		}
	}
	~WinsockGuard() {
		WSACleanup();
	}
};
```

저번 글(RAII)에서 작성해봤던 코드에다가
만약 윈속 초기화 과정에서 에러가 발생한다면(WSAStartup() != 0)
에러 코드까지 출력하도록 클래스를 만들었어.

일단 WinsockGuard 객체를 생성한다면,
생성자가 호출되어서 WSAStartup() 함수를 호출해서 자동으로 윈속 초기화를 해줘.

그리고 해당 객체가 스코프를 벗어나서 소멸자가 호출된다면,
WSACleanup() 함수가 호출되어서 자동으로 윈속 종료를 해주지.

일단, 보다시피 여기에는 RAII를 적용했어.
만약 해당 객체가 스코프를 벗어난다면,
자동으로 WSACleanup()을 호출되게 해서,
혹시 main() 함수 종료 전에 WSACleanup()을 호출하는걸 까먹는 상황을 방지했어.

그리고 중간에 예외가 발생해서 프로그램을 종료하게 되어도, 
자동으로 소멸자가 호출되어 WSACleanup()을 호출하게 되어서
무조건 윈속 자원은 정리될 수 있게 했어.

그리고 왜 에러 코드를 출력하는 코드를 추가했냐면, 
기존 코드에서도 에러 코드까지 출력하고 있어서 추가한거야.
리팩토링의 목적은 같은 기능을 하는 코드를 고치는 거니까.

그리고 std::cerr은 에러 메시지 출력 전용 스트림이라고 해서,
일단 여기서 사용해봤어.

근데 잘 한건지는 모르겠네. 이런 곳에서 사용하는게 아닌 것 같기도..

### 1-2-2. ListenSocket

```cpp
class ListenSocket {
private:
	SOCKET listen_sock;

public:
	ListenSocket() {
		listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock == INVALID_SOCKET) {
			err_display("socket()");
			throw std::runtime_error("socket() 함수 실패");
		}
	}

	ListenSocket(const ListenSocket& s) = delete;
	ListenSocket& operator=(const ListenSocket&) = delete;

	void ListenSockBind(sockaddr_in* addr) {
		if (bind(listen_sock, (sockaddr*)addr, sizeof(*addr)) == SOCKET_ERROR){
			err_display("bind()");
			throw std::runtime_error("bind() 함수 실패");
		}
	}

	void ListenSockListen() {
		if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
			err_display("listen()");
			throw std::runtime_error("listen() 함수 실패");
		}
	}

	ClientSocket ListenSockAccept(sockaddr_in* client_addr) {
		int len = sizeof(*client_addr);
		SOCKET client_sock = accept(listen_sock, (sockaddr*)client_addr, &len);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			throw std::runtime_error("accept() 함수 실패");
		}

		return ClientSocket(client_sock); // 여기 해결 필요함
	}

	~ListenSocket() {
		if (listen_sock != INVALID_SOCKET) {
			closesocket(listen_sock);
		}
	}
};
```

이 클래스는 서버가 클라이언트로부터 연결 요청을 받는 소켓인
listen용 소켓(기존에는 server_sock, 현재에는 listen_sock)에 대한 클래스야.

일단 listen용 소켓을 생성자로 생성하고,
private 접근 지정자로 객체 외부에서 접근하는 경우를 차단함으로써
해당 소켓의 소유권을 분명히 하고,
listen_sock은 이 객체의 메서드를 통해서만 사용될 수 있는 구조를 만들었어.

그리고 listen용 소켓을 사용해서 하는 일인
- bind()
- listen()
- accept()

는 전부 ListenSocket 객체에 묶었어.

- bind() - ListenSockBind()
- listen() - ListenSocklisten()
- accept() - ListenSockAccept()

이렇게.

어차피 listen용 소켓에 RAII를 적용하는 객체인데,
listen용 소켓이 사용되던 일까지
전부 해당 객체에 넣어버리자는 생각으로
이렇게 했어.

그러니까, 자원 중심으로 객체를 만든거야.

그리고 발생할 수 있는 예외는
전부 throw를 사용해서 이 객체가 생성된 함수로 던졌어.
그 함수에서 throw로 던져진 예외를 try-catch문으로
처리를 해줘야겠지.

그리고 소멸자에 closesocket(listen_sock)으로 
스코프에서 벗어나면 자동으로 listen_sock을 닫음으로써,
혹시 실수로 listen_sock을 닫지 않을 경우를 차단했어.

그리고, 만약 이 객체가 복사된다면 
복사된 객체까지 소멸될 때 
closesocket() 함수를 두 번 호출하는 경우가 생길 수 있어서,
이 객체는
```cpp
ListenSocket(const ListenSocket& s) = delete;

```
이렇게 해당 객체를 복사하는 것과,
```cpp
ListenSocket& operator=(const ListenSocket&) = delete;
```
해당 객체를 대입해서 복사하는 것을
차단했어.

### 1-2-3. ClientSocket

```cpp
class ClientSocket {
private:
	SOCKET client_sock;
public:
	ClientSocket(SOCKET s) : client_sock(s) {}

	ClientSocket(const ClientSocket&) = delete;
	ClientSocket& operator=(const ClientSocket&) = delete;

	int ClientSockSend(NetState& state, const char* msg, int len) {

		int send_res = send_all(client_sock, state, msg, len);
		if (send_res == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}

		return send_res;
	}

	int ClientSockRecv(NetState& state, char* buf, int len) {

		int recv_res = recv_all(client_sock, state, buf, len);
		if (recv_res == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}

		return recv_res;
	}

	~ClientSocket() {
		if (client_sock != INVALID_SOCKET) {
			closesocket(client_sock);
		}
	}
};
```
이 클래스는 listen용 소켓이 accept()로 클라이언트의 연결을 수락했을 때
생성되는 클라이언트와 통신하는 용도의 소켓(client_sock)에 대한 클래스야.

일단 client_sock을 생성자를 사용해서
기존의 ListenSocket객체의 listen_sock이 ListenSockAccept() - accept()해서 나온 소켓으로 초기화를 해.
```cpp
ClientSocket(SOCKET s) : client_sock(s) {}
```
이렇게. 저 뒤에 `: client_sock(s)`라는건 
ClientSocket() 생성자로 받은 소켓으로 client_sock 소켓을 초기화하라는 뜻이야.
C++은 초기화와 대입이 엄격하게 구분되어 있어서,
외부에서 자원을 받아올 때는 이렇게 생성자로 초기화를 해주는게 좋아.
C++에서는 멤버 변수는 생성자 본문이 아니라 초기화 리스트에서 초기화하는 것이 권장돼.
특히 소켓처럼 자원을 나타내는 핸들은 
객체 생성과 동시에 유효한 상태로 만들어주는 것이 중요하다고 해.

그리고 물론 client_sock도 private 접근 지정자로 해당 객체 외부에서 접근할 수 없게 해서
해당 소켓의 소유권을 분명히 하고,
client_sock은 이 객체의 메서드를 통해서만 사용될 수 있는 구조를 만들었어.

그리고 ListenSocket에서처럼 여기에서도
클라이언트와 통신하는 용도의 소켓을 사용해서 하는 일인
- send()
- recv()

를 전부 이 객체에 묶었어.

- send() - ClientSockSend()
- recv() - ClientSockRecv()

이렇게.

여기에서도 어차피 클라이언트와 통신하는 용도의 소켓에 RAII를 적용할 뿐인 객체인데,
기존의 클라이언트와 통신하는 용도의 소켓이 사용되던 일까지
전부 해당 객체에 넣어버리자는 생각으로
이렇게 했어.

그리고 어차피 ClientSockSend() / ClientSockRecv() 둘 다
그저 지정된 바이트 열을 송 / 수신해주는 함수라,
굳이 client_sock이 필요하지 않은 직렬화 / 역직렬화는 따로 객체 내에 함수를 만들지 않았어.

클래스에 대한 외부에서의 의존성을 최대한 줄이고 싶었거든.
그저 ClientSocket 클래스는 client_sock에 관한 일만 처리하게 했어.

그리고 기존의 send() / recv() 과정 모두 throw로 예외를 상위 함수에 던지는 방식은
예외가 발생했을 때 while 루프 밖으로 나가지 않고 
계속 송 / 수신 이후 코드가 실행되었어야 했기 때문에 맞지 않았어서,
여기서는 단순히 `SOCKET_ERROR` / `송 / 수신한 바이트 수`를 반환하는 것으로 처리했어.

그리고 여기서도 물론 소멸자로 
객체가 스코프에서 벗어났을 때 자동으로 closesocket()이 호출되어서
해당 소켓 핸들을 무효화하게 했어.

그리고, 이 객체도 ListenSocket 클래스와 같은 이유로 복사하면 안되므로
```cpp
ClientSocket(const ClientSocket&) = delete;
ClientSocket& operator=(const ClientSocket&) = delete;
```
이렇게 같은 방식으로 복사를 차단했어.

## 2. 리팩토링 과정에서 발생한 버그들과 디버깅

리팩토링 과정에서도 정말 많은 버그가 발생했어.
정말.. 정말 많고 기상천외한ㅋㅋ
그리고 이 버그들은 단순히 IDE에서 처음부터 보이는게 아니라
내가 직접 빌드 해봤을 때 나왔던 버그들이 대부분이야.

### 2-1. 클래스 / 구조체 / 함수의 선언 순서 문제

일단 가장 먼저 마주친 버그는 
ListenSocket 클래스의 
ClientSocket 클래스와 NetState 구조체를 사용하는 부분, 
그리고 ClientSocket 클래스의 NetState 구조체, recv_all() / send_all() 함수를 사용하는 부분이었어.

이게 결국 왜 그랬냐면,
C++은 위에서부터 아래로 코드를 읽는데,
ListenSocket 클래스보다 해당 클래스 내에서 사용하는 ClientSocket 클래스와 NetState 구조체가
더 아래에 있어서
ListenSocket 클래스를 컴파일 할 때 ClientSocket과 NetState 구조체를 컴파일러는 모르니
해당 인스턴스를 찾을 수 없다는 버그가 발생했던거야.

그래서 이거는 순서를 
위에서 아래로 NetState 구조체 -> ClientSocket 클래스 -> ListenSocket 클래스 순서로 배치해서 해결했는데..

일단 이 버그로 얻은 교훈은,
C++에서 특정 객체 / 함수에 의존성이 있는 객체 등은
그 객체 / 함수들이 그 객체보다 위에 있는지 확인하는 습관을 들이자..

### 2-2. 앞의 구조체 / 함수들을 헤더 파일로 옮기면서 생긴 중복 정의 문제

그리고 여기서, 내 생각이 어차피 자주 사용할 구조체 / 함수들이니 
아예 별개의 헤더 파일로 옮겨서 사용하자는 생각이었어.
어차피 전처리기로 해당 헤더 파일을 컴파일 하기 전에 미리 읽을테니까
앞에서 말한 순서 문제는
그저 ClientSocket 클래스와 ListenSocket 클래스의 순서만 주의해주면 된다고 생각했어.

그래서 NetState 구조체, recv_all() / send_all() 함수를
이미 PacketHeader 관련된 구조체 / 열거형이 저장되어있던 
헤더 파일(NetCommon.h)로 옮겼지.

하지만 다시 컴파일해보니,
컴파일 에러가 발생했어.

왜 그랬냐면, 이게 헤더 파일(NetCommon.h)과 소스 파일(Server.cpp) 둘 다에
NetState 구조체, recv_all() / send_all() 함수가 정의되어 있었기 때문에,
컴파일러가 어떤 구조체 / 함수가 NetState 구조체이고 recv_all() / send_all() 함수인지
못 찾았던 거였어.
이름이 중복되어있었으니까.

그래서 이 버그로 얻은 교훈은,
다른 파일로 분리할 때는 기존 코드에 남아있던 분리된 코드들은 지우자.
다른 파일로 분리 / 기존 코드 삭제를 묶어서 생각하자.
이 정도.

### 2-3. enum class PacketType 도입 후 직렬화 코드와 충돌한 문제

나는 직렬화 / 역직렬화를 할 때 htonl() / ntohl() 함수를 사용해.
그리고 이 두 함수는 인자로 정수를 기대하고.

하지만, 결국 헤더의 타입,
열거형 PacketType은 그저 PacketType일 뿐 정수형이 아니라서 문법 에러가 발생했어.

전에도 설명했듯, enum class는 기존 enum의 단점인 이름 중복 문제를 해결하기 위해
따로 이렇게 PacketType::Safe 앞에 형식 지정자를 붙여야되었고,
비교할 때 int로 취급해버리는 문제를 막기 위해 사실상 enum class의 이름으로
형식을 잡는 느낌이었어.

하지만 결국 이런 점들이 htonl() / ntohl() 함수를 쓸 때는 오히려 방해였던 것 같아..

일단 나는 지금 당장은 이걸 근본적으로 해결할,
enum class를 쓰면서 htonl() / ntohl() 함수를 사용할 발상이 잘 안 떠올라서,
땜빵 차원에서 PacketType 열거형을 포기하고
단순히
```cpp
const int32_t HEADER_ERROR = 0;
const int32_t SAFE = -1;
```
이렇게 int32_t 형의 상수로 정의해서 해결했어.

다음에는 이 문제를 꼭 해결해보고 싶어.

### 2-4. ListenSocketAccept() 함수와 ClientSocket 클래스의 복사 금지 충돌

이건 아직 해결하진 못한 문제야.

뭐가 문제냐면, ClientSocket 클래스에는
```cpp
ClientSocket(const ClientSocket&) = delete;
ClientSocket& operator=(const ClientSocket&) = delete;
```
이렇게 해당 객체의 복사를 금지하는 코드를 작성해놨었어.

문제는 ListenSockAccept()에서 이 객체를 값으로 반환할 때 생겼다.
```cpp
return ClientSocket(client_sock);
```
내 입장에서는 그냥 SOCKET을 넣어서 새 ClientSocket 객체를 만들어 반환하는 것처럼 보였는데,
컴파일러는 이 반환 과정에서 복사/이동 생성 가능성을 함께 본다고 해.
그런데 복사는 막아놨고, 이동 생성자는 따로 정의하지 않았으니 문제가 생길 수 밖에 없었지..

앞에서도 말했듯이 이건 아직 해결하지는 못한 문제야.
그래도 리팩토링이 전부 끝날 때까지는 해결해보려고.

이동 생성자를 따로 정의해서 해당 객체를 복사가 아닌 이동시키는 식으로..

### 2-5. 코드 문제가 아니라 인코딩 문제였던..

이건 진짜 괴상했어.

NetCommon.h 헤더 파일에서 발생했던 오류였는데,
![](https://velog.velcdn.com/images/siryus0907/post/5e0a8aeb-0eea-492e-b9af-e1ce4ea2601b/image.png)

이게 무슨ㅋㅋ

이게 하나 빼고 다 같은 라인(줄)에서 발생했어ㅋㅋ 

결국 지피티 / 제미나이에게 물어보고, 구글링도 해보면서
원인을 찾았는데,

이게 저 맨 위에 있던 인코딩으로 표시할 수 없는 문자 뭐시기 경고..
이게 핵심이었어.

이게 인코딩 방식이 달라서,
기존에 인코딩 방식이 달랐던 내 코드에서는 멀쩡하게 문자로 잘 표현이 되었던 코드가
이 인코딩 방식의 NetCommon.h 파일에서는 일부 지원이 안 되었기 때문에,
이 오류가 발생했던거였어ㅋㅋ

그래서 결국 문제가 되었던 67번째 줄과 69번째 줄을 싸그리 다시 적으니,
이 오류가 해결됐어ㅋㅋ

이 오류로 얻은 교훈은,
결국 내 코드에 문제가 없어도 환경, 인코딩, 컴파일러 등에 따라서
문제가 생길 수 있다.
디버깅 할 때 내 코드만 볼게 아니라 코드가 실행되는 환경, 코드가 인코딩되어있는 방식,
코드를 컴파일하는 컴파일러 등도 살펴봐야 한다는 것.
정도로 요약할 수 있겠네.