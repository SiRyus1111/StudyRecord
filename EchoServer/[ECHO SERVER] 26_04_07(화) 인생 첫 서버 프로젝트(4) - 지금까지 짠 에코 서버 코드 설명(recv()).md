## 0. 서론

지난번엔 accept() 로 클라이언트의 접속 요청을 받는 부분까지 설명했으니,
이젠 기본적인 소켓 함수 기준으로는 recv() / send() / closesocket()만 남았네.

하지만 recv() 까지는 지금까지 짠 코드 내에서 대략적으로 완성되어있지만,
send() / closesocket()은 사실상 하나도 완성되어있지 않아.
딱 테스트용 코드.
서버와 클라이언트가 통신하는 것만 보고 싶어 짰던 코드여서.
그리고 이거 내가 짠게 아니라 사실상 Ctrl + C / Ctrl + V한거야.

그래서 이번 글에서는 recv()까지 설명하고,
중간에 헤더 해석 / send() 부터는 
직접 해당 기능을 구현한 후에 정리하는 식으로
글을 쓸 것 같아.

그리고 추가로 미완성인 부분인
- 연결 종료 처리(closesocket())
- 다양한 예외처리
- 클라이언트

이 부분들도 구현한 후에 정리하는 식으로..

일단 지금까지 구현한 전체 코드 올려놓을게.
다시 말하지만 이거 미완성 코드라,
곧곧에 로직이 빈 부분과 오류는 충분히 있을 수 있어.

```cpp
#include <iostream> // 콘솔 입출력 용 - cout, cin, ...
#include <winsock2.h> // 윈속2 메인 헤더 - socket(), bind(), listen(), accept(), recv(), send(), ...
#include <ws2tcpip.h> // 윈속2 확장 헤더 - inet_ntop(), inet_pton(), ...
#include "Common.h" // 사용자 정의 라이브러리. 소켓 함수 오류 출력 함수 포함. err_quit(), err_display() 함수는 Common.h에 정의되어 있음.
// #include <cstdio> / 이거 왜 썼을까? / 일단 지금은 이 라이브러리가 있었다는 기록만 남겨둠. 주석 처리. 주석 처리.
#include <cstdlib> // atoi() 함수 사용하기 위해서
#include <cstring> // memcpy() 함수 사용하기 위해서

const int SERVER_PORT = 9000;
const int BUFFER_SIZE = 4096;
const int HEADER_SIZE = 4;

// 서버가 할 일 (클라이언트와 연동)
// 1. 클라이언트로부터 uint32_t형의 헤더를 받아 헤더의 값만큼 페이로드 받기
// 2. 처리하고, 클라이언트의 uint32_t형의 보낸 페이로드 바이트 수를 나타내는 헤더 전송하기
// 3. 연결을 끊을 때.. 어카면 좋을까..?

#pragma comment(lib, "Ws2_32.lib")

// 상태 관리 구조체
struct flags {

	bool header_recv = false;
	bool payload_recv = false;
	bool header_send = false;
	bool payload_send = false;

	bool if_error = false;
	bool if_client_exit = false;

};

int main() {
	WSADATA wsa;
	int WSAStartup_result = WSAStartup(MAKEWORD(2, 2), &wsa);

	if (WSAStartup_result != 0) {
		std::cout << "윈속 초기화 실패. 에러 코드 : " << WSAStartup_result << '\n';
		return 1;
	}


	SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // LISTEN용 소켓

	if (server_sock == INVALID_SOCKET) {
		err_quit("socket()");
		return 1;
	}

	sockaddr_in server_addr{}; // LISTEN용 소켓의 소켓 주소 구조체

	// 서버 주소 정보 설정
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// LISTEN용 소켓에 서버 주소 정보 바인딩 
	if (bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		err_quit("bind()");
		return 1;
	}

	// LISTEN용 소켓을 LISTEN 상태로 전환, 백 로그 크기는 가능한 최대 크기인 SOMAXCONN으로 설정
	if (listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
		err_quit("listen()");
		return 1;
	}

	// 클라이언트와 직접적으로 통신할 소켓과 소켓 주소 구조체
	SOCKET client_sock;
	sockaddr_in client_addr{};

	// 클라이언트로부터 수신한 메시지를 저장할 버퍼
	// 버퍼의 마지막 바이트는 문자열이 끝나는 지점을 나타내는 널 문자('\0')를 저장.
	char buf[BUFFER_SIZE];
	int addr_len;


	// 클라이언트의 접속을 기다리고, 접속이 이루어지면 메시지를 수신한 후 다시 클라이언트로 송신하는 에코 서버
	while (true) {

		addr_len = sizeof(client_addr);

		// accept 함수를 실행. accept 함수는 블로킹임. 그래서 연결 정보가 백로그 큐에 들어올 때까지 이 부분에서 멈춰있음. 이 함수를 지나쳐갈까 걱정할 필요 X
		client_sock = accept(server_sock, (sockaddr*)&client_addr, &addr_len);

		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			continue; // accept() 실패했을 때는 클라이언트와의 연결이 이루어지지 않은 상태이므로, 다음 반복으로 넘어가서 다시 accept() 시도
		}

		// 출력용으로 클라이언트 IP 주소를 문자열로 저장
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_addr.sin_addr, addr, sizeof(addr));

		std::cout << "클라이언트 접속됨 : IP 주소 = " << addr << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';

		struct flags flags;

		while (true) {


			// 헤더 recv()
			int header_received = 0;
			char header_buf[4]{};

			flags.header_recv = true;
			while (header_received < HEADER_SIZE)
			{
				int header_recv_len = recv(client_sock, header_buf + header_received, HEADER_SIZE - header_received, 0);

				if (header_received != HEADER_SIZE) {
					std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
				}
				if (header_recv_len == SOCKET_ERROR) {
					err_display("recv()");
					flags.if_error = true;
					break;
				}
				else if (header_recv_len == 0) {
					flags.if_client_exit = true;
					break;
				}

				header_received += header_recv_len;
			}

			if (flags.if_error || flags.if_client_exit) break;

			flags.header_recv = false;

			std::cout << "헤더 수신 완료 : 총 " << header_received << "바이트 수신됨.\n";



			uint32_t net_header;
			memcpy(&net_header, header_buf, HEADER_SIZE);

			// 보냈을 때 네트워크 바이트 정렬로 보냈으니까 받았을 때 다시 호스트 바이트 정렬로 변환 + 형식도 uint32_t로 유지
			uint32_t host_header = ntohl(net_header);

			// 페이로드 recv()
			int payload_received = 0;

			flags.payload_recv = true;
			while (payload_received < host_header) {

				int recv_len = recv(client_sock, buf + payload_received, host_header - payload_received, 0);

				if (recv_len == SOCKET_ERROR) {
					err_display("recv()");
					flags.if_error = true;
					break;
				}
				else if (recv_len == 0) {
					flags.if_client_exit = true;
					break;
				}

				payload_received += recv_len;
			}
			buf[payload_received] = '\0';

			std::cout << "송신한 클라이언트 : IP 주소 = " << ntohl(client_addr.sin_addr.s_addr) << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';
			std::cout << "받은 바이트 수 : " << payload_received << " 받은 메시지 : " << buf << '\n';

			// 헤더 send(), 페이로드 send()
			int send_len = send(client_sock, buf, host_header, 0);

			if (send_len == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			std::cout << addr << " : " << htons(client_addr.sin_port) << " 클라이언트로 " << send_len << " 바이트 보냄\n";
		}

	}

	closesocket(server_sock);

	return 0;
}
```

[최신 깃허브 링크](https://github.com/SiRyus1111/My-First-Echo-Server-Project)

일단 이게 첫 프로젝트라 코드의 질은 썩 좋진 않아.
그래도 내가 이 코드를 왜 적었는지 설명은 할 수 있으니까,
이 정도면 첫 프로젝트로는 괜찮다고 생각해.

물론 리팩토링도 해볼거고.

## 1. 수신 과정

> 이 프로젝트에서의 수신 과정 = 지금까지 내가 짠 코드 내에서는 recv() 파트

라고 할 수 있지.

그래서 지금 당장은 recv() 파트만 볼 생각이야.
나중에 수신 과정에서의 예외 처리나 추가적으로 설계해야할 프로토콜같은게 있다면 
충분히 recv() 과정 내에 추가될 수 있겠지.

그리고 recv() / send() 같은 반복되는 과정은 
나중에 함수로 빼서 코드의 가독성을 높일 생각이야.


이 코드는 송수신 파트의 전체 코드이지만,
send() 파트는 좀 간략화해놨어.

지금은 recv() 중심이니까.

일단 지난번 글의 마지막 상태가,
1. WSAStartup() - 윈속 초기화
2. socket() - server_sock 생성
3. bind() - 모든 IPv4 주소에서 접속 가능, 9000번 포트를 통해 접속 가능하게 server_sock에 바인딩
4. listen() - server_sock을 LISTEN(LISTENING) 상태로 전환해서 클라이언트 연결 대기
5. 첫 번째 while문 진입(서버 종료시까지 이 while문 밖에는 안나감)
6. accept() 로 클라이언트와 통신할 소켓 핸들(client_sock) 생성
7. 상태 관리 구조체 flags 생성(이건 이름 바꾸는게 좋아보여. 구조체 이름 == 변수 이름이기 때문에.)

이 정도까지 진행이 됐었어.

이 상태에서 현재 코드가 진행이 된다고 생각하면 돼.

```cpp
while (true) {


	// 헤더 recv()
	int header_received = 0;
	char header_buf[4]{};

	flags.header_recv = true;
	while (header_received < HEADER_SIZE)
	{
		int header_recv_len = recv(client_sock, header_buf + header_received, HEADER_SIZE - header_received, 0);

		if (header_received != HEADER_SIZE) {
			std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
		}
		if (header_recv_len == SOCKET_ERROR) {
			err_display("recv()");
			flags.if_error = true;
			break;
		}
		else if (header_recv_len == 0) {
			flags.if_client_exit = true;
			break;
		}

		header_received += header_recv_len;
	}

	if (flags.if_error || flags.if_client_exit) break;

	flags.header_recv = false;

	std::cout << "헤더 수신 완료 : 총 " << header_received << "바이트 수신됨.\n";



	uint32_t net_header;
	memcpy(&net_header, header_buf, HEADER_SIZE);

	// 보냈을 때 네트워크 바이트 정렬로 보냈으니까 받았을 때 다시 호스트 바이트 정렬로 변환 + 형식도 uint32_t로 유지
	uint32_t host_header = ntohl(net_header);

	// 페이로드 recv()
	int payload_received = 0;

	flags.payload_recv = true;
	while (payload_received < host_header) {

		int recv_len = recv(client_sock, buf + payload_received, host_header - payload_received, 0);

		if (recv_len == SOCKET_ERROR) {
			err_display("recv()");
			flags.if_error = true;
			break;
		}
		else if (recv_len == 0) {
			flags.if_client_exit = true;
			break;
		}

		payload_received += recv_len;
	}

	if (flags.if_client_exit || flags.if_error) break;

	buf[payload_received] = '\0';

	std::cout << "송신한 클라이언트 : IP 주소 = " << ntohl(client_addr.sin_addr.s_addr) << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';
	std::cout << "받은 바이트 수 : " << payload_received << " 받은 메시지 : " << buf << '\n';

	// 헤더 send(), 페이로드 send()
	int send_len = send(client_sock, buf, host_header, 0);

	if (send_len == SOCKET_ERROR) {
		err_display("send()");
		break;
	}
	std::cout << addr << " : " << htons(client_addr.sin_port) << " 클라이언트로 " << send_len << " 바이트 보냄\n";
}
```

이제 recv()의 첫 과정인,
헤더 recv() 파트부터 보자.

### 1-1. 헤더 recv()

```cpp
// 헤더 recv()
int header_received = 0;
char header_buf[HEADER_SIZE]{};

flags.header_recv = true;
while (header_received < HEADER_SIZE)
{
	int header_recv_len = recv(client_sock, header_buf + header_received, HEADER_SIZE - header_received, 0);

	
	if (header_recv_len == SOCKET_ERROR) {
		err_display("recv()");
		flags.if_error = true;
		break;
	}
	else if (header_recv_len == 0) {
		flags.if_client_exit = true;
		break;
	}

	header_received += header_recv_len;

	if (header_received != HEADER_SIZE) {
		std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
	}
}

if (flags.if_error || flags.if_client_exit) break;

flags.header_recv = false;

std::cout << "헤더 수신 완료 : 총 " << header_received << "바이트 수신됨.\n";
```

이렇게 돼.
일단 처음부터 설명해볼게.

---
```cpp
int header_received = 0;
char header_buf[4]{};
```

일단 header_received는 
TCP의 바이트스트림 프로토콜이라는 특성 + 기타 이유(흐름 제어 / 혼잡 제어 / 클라이언트 송신 버퍼의 공간 부족 등)로
비록 4바이트짜리더라도 헤더가 한 번에 안 올 수 있기 때문에,
지금까지 헤더를 총 몇 바이트를 받았는지 기록해서,
헤더를 다 받으면 recv() 를 계속 호출하는 while문이 종료되게 하기 위해서
만든 변수야.

그리고 header_buf는 받은 헤더를 넣을 버퍼야.
헤더의 길이는 4바이트로 고정되어있기 때문에,
굳이 맨 끝에 널문자(\0)를 붙일 필요가 없다고 생각해서 
딱 4바이트로 버퍼의 크기를 잡았어. 

그런데 저기 4바이트라는 것을 하드코딩으로 박아넣는 것 보다는 
미리 전역 상수로 선언해놨던 HEADER_SIZE를 넣는게 좋아보이는데..
그래서 이렇게 직접 전역 상수 HEADER_SIZE를 직접 넣는 식으로 수정했어.
```cpp
int header_received = 0;
char header_buf[HEADER_SIZE]{};
```
이렇게.
<br>

이제 이렇게 상태 관리 구조체의 header_recv 멤버를 true로 바꿔서
확실히 현재 header_recv(헤더 수신) 단계에 접어들었다는 것을 명시해줘.

```cpp
flags.header_recv = true;
```
---

이제 실제로 recv() 함수를 호출하는 단계야.
```cpp
while (header_received < HEADER_SIZE)
{
	int header_recv_len = recv(client_sock, header_buf + header_received, HEADER_SIZE - header_received, 0);

	
	if (header_recv_len == SOCKET_ERROR) {
		err_display("recv()");
		flags.if_error = true;
		break;
	}
	else if (header_recv_len == 0) {
		flags.if_client_exit = true;
		break;
	}

	header_received += header_recv_len;

	if (header_received != HEADER_SIZE) {
		std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
	}
}
```

일단 while문의 종료 조건부터.
```
지금까지 받은 헤더 < 실제 헤더 크기
```
인 경우에 반복해.
그러니까, 헤더를 다 받지 못했다면 반복하는거야.
그런데 지금까지 받은 헤더는 어떻게 알 수 있을까?
이건 계속 코드를 살펴보며 설명할게.

---

이제 진짜 헤더 수신인 recv()를 호출해.
client_sock으로 데이터를 받고,
그러니까 수신 버퍼의 바이트열을 꺼내오고.

그리고 두 번째 원소는 받은 바이트열을 저장할 버퍼의 주소값을 넣어야해.
나는 포인터 연산으로 
```
header_buf(해당 버퍼의 첫 번째 주소) + header_received(지금까지 받은 바이트 수)
```
이렇게 해서, 
만약 2바이트째까지 받아서 header_received == 2라면,
두 번째 원소는 header_buf + 2이고,
그러므로 header_buf\[2\]의 주솟값을 가리키고 있겠지.

그래서 세 번째 바이트를 2번 인덱스, 즉 세 번째 인덱스에 받을 수 있는거야.

세 번째 원소는 이번 recv()로 받기를 희망하는 바이트 수를 넣어야하는데,
나는 
```cpp
HEADER_SIZE - header_received
```
이렇게 했어.
만약 1바이트째까지 받아서 header_received == 1이라고 가정해보면,
HEADER_SIZE는 4(바이트)이므로 앞으로 3바이트를 더 받아야하겠지.
거기서
HEADER_SIZE - header_received를 해주면,
4 - 1이므로 3이 나와서 앞으로 3바이트를 더 받을 수 있게 해줘.

그리고 이번엔 데이터가 확실히 여러 개로 나눠어져 온다는걸 체감해보고 싶어서
MSG_WAITALL같은 플래그는 사용하지 않았어.

그리고 recv() 의 반환값, 즉
> recv() 로 처리한 바이트 수

는 header_recv_len이라는 int형 변수에 저장해줘.
recv()는 int형으로 반환하니까.

--- 

이제 recv()에서 발생할 수 있는 예외에 대한 예외처리 + header_received 값 갱신이야.

```cpp
if (header_recv_len == SOCKET_ERROR) {
	err_display("recv()");
	flags.if_error = true;
	break;
}
else if (header_recv_len == 0) {
	flags.if_client_exit = true;
	break;
}

header_received += header_recv_len;

if (header_received != HEADER_SIZE) {
	std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
}
```

일단 recv() 반환값(header_recv_len) == SOCKET_ERROR이라면,
recv() 함수 실행 도중에 에러가 발생했음을 알 수 있어.
그러니 err_display()로 WSAGetLastError()를 호출 + 오류를 출력해주고,
상태 관리 구조체의 if_error 멤버를 에러가 발생했다는 뜻인 true로 바꿔줘.
그리고 해당 반복 recv() 루프를 탈출하지.

아니면 recv() 반환값(header_recv_len) == 0이라면,
클라이언트로부터 정상적인 연결 종료(closesocket() 호출 등)가 일어났음을 알 수 있어.
그러니 상태 관리 구조체의 if_client_exit 멤버를 클라이언트가 연결 종료했다는 뜻인 true로 바꿔.
그리고 해당 반복 recv() 루프를 탈출하지.
```cpp
if (header_recv_len == SOCKET_ERROR) {
	err_display("recv()");
	flags.if_error = true;
	break;
}
else if (header_recv_len == 0) {
	flags.if_client_exit = true;
	break;
}
```
이렇게.
<br>
이제 지금까지 받은 헤더의 총 바이트 수를 갱신해줘.
```cpp
header_received += header_recv_len;
```
이거는, 
```
지금까지 처리한 바이트 수 += 이번 recv() 루프에서 처리한 바이트 수;
```
라고 생각하면 돼.

이렇게 지금까지 받은 헤더의 총 바이트 수를 갱신하는거야.

그리고, 헤더가 완전히 전송되지 않았다면(header_received != HEADER_SIZE)
헤더 부분적 수신 알림을 따로 출력해줬어.
```cpp
if (header_received != HEADER_SIZE) {
	std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
}
```
이걸 왜 다른 recv() 함수 예외처리와 같이 하지 않고,
지금까지 받은 헤더의 총 바이트 수를 갱신한 후에 진행하냐면,
결국 부분적 수신이라는걸 판단할 수 있는 조건이
내 로직 상으로 여러번 판단하려면 header_received를 보고 판단할 수 밖에 없는데,

만약 처음에 4바이트가 전부 전송되었다면,
```cpp
header_received (0) += recv_header_len (4)
```
이렇게 먼저 header_received를 4로 갱신해야지만
header_received != HEADER_SIZE 이 조건이 성립되지 않아서
헤더 부분적 수신 알림이 출력이 안 돼.

만약 `header_received` 갱신 전에 
`header_received != HEADER_SIZE` 이 조건을 먼저 검사했다면,
부분적으로 수신되지 않았음에도
아직 `header_received`가 갱신되지 않았기 때문에
부분적 수신 알림을 출력하게 돼.

---

그리고 이제 HEADER_SIZE(4바이트)만큼 헤더를 수신했거나,
수신 중간에 에러가 발생해서 break하는 식으로
while문의 반복 조건이 달성되지 않게 되어서 while문 밖으로 나갔다고 하자.

그러면 이제 이 코드들이 실행돼.

```cpp
if (flags.if_error || flags.if_client_exit) break;

flags.header_recv = false;

std::cout << "헤더 수신 완료 : 총 " << header_received << "바이트 수신됨.\n";
```

먼저 예외 상황에 대해 판단을 해.
이게 정상적으로 헤더를 수신했다면,
flags.if_error와 flags.if_client_exit 둘 다 false(기본값)가 되겠지.

그래서 오직 중간에 if문에서 판별한 **recv() 함수의 에러가 발생했거나**,
**클라이언트가 연결을 종료한 경우**에는
이 if문 안으로 들어오게돼.
그리고 한번 더 break하지.
accept()를 호출했던 while문으로 나가게 돼.

그리고 이제 헤더 수신 과정이 끝났고 예외 상황도 없었으니 
상태 관리 구조체(flags)의 header_recv 멤버를 false로 바꿔.

그리고 헤더 수신 완료 메시지를 출력해.
물론 총 수신한 바이트 수(header_received)도 출력해서,
혹시 헤더 수신할 때의 로직에 이상이 없나 디버깅 할 수도 있어.

--- 

일단 이 단계는 recv() 해야하는 페이로드의 크기를 알려주는 헤더를 
먼저 고정 길이만큼 recv() 하는 단계야.
이제 이 헤더를 해석한 후에, 해석한 값만큼 페이로드를 recv() 해주면 돼.

그리고 이 시점에서
상태 관리 구조체(flags)는 정상적으로 진행되었다면 모든 값이 false이고,
header_buf에는 4바이트의 네트워크 바이트 정렬인 클라이언트에서 보낸 헤더가 담겨있어.

> 요약 : recv() 해야하는 페이로드의 크기를 알기 위해 먼저 헤더를 recv() 한 상태이다.
이제 헤더를 해석한 후 해석한 값에 맞춰서 페이로드를 recv() 하면 된다.

---

### 1-2. 헤더 해석

이제 header_buf에 담겨있는 4바이트 네트워크 바이트 정렬의 헤더를 해석해야겠지.
```cpp
// 해더 해석
uint32_t net_header;
memcpy(&net_header, header_buf, HEADER_SIZE);

// 보냈을 때 네트워크 바이트 정렬로 보냈으니까 받았을 때 다시 호스트 바이트 정렬로 변환 + 형식도 uint32_t로 유지
uint32_t host_header = ntohl(net_header);
```

헤더 해석 파트는 이 세 줄의 코드로 해결이 돼.
일단 네트워크 바이트 정렬의 unsigned int 32bit 형의 헤더를 받을 net_header 변수를 선언하고,
memcpy() - memory copy 함수로 header_buf의 바이트들을
net_header의 주소로 복사해.
물론 HEADER_SIZE만큼만.
그런데 여기는 HEADER_SIZE보단 직접 header_buf의 크기를 sizeof()으로 재서 넣는게 나았을 수도 있을 것 같네. 
하지만 아직은 둘 다 같은 의미인 것 같아서, 일단 HEADER_SIZE를 그대로 유지할게.

그런데, 클라이언트에서는 네트워크 바이트 정렬(빅 엔디언)으로 헤더를 송신했어.
그러니, 서버에서는 네트워크 바이트 정렬인 헤더를 다시 호스트 바이트 정렬로 변환해서 해석해줘야겠지.

그래서 memcpy() 를 하고난 후에 바로 
ntohl() - Network to Host Long 함수로 호스트 바이트 정렬로 변환해서 
uint32_t(unsigned int 32bit) 형의 변수인 host_header에 넣어줘.

전엔 호스트 바이트 정렬로 변환하기 전의, 바이트열 -> 숫자 바꾸는 함수로
atoi() - ASCII to integer을 썼었는데,
우리는 문자열 -> 숫자 변환이 아닌,
바이트열 -> 숫자 변환을 의도하고 있으니까,
memcpy() 를 썼어.

그리고 아직 안 만든 부분이긴 한데,
만약 헤더를 해석한 값이 최대 버퍼 크기인 4096을 넘으면 안될 것 같아.
그래서 그럴 때는 따로 처리를 해야할 것 같네.
이건 나중에 다양한 예외처리를 구현할 때 건드려볼게.
클라이언트에게 오류를 알려주기 위해서 프로토콜을 추가로 설계해야할 수도 있을 것 같아서 
지금 건드리긴 좀..

--- 

일단 이 단계는 recv() 해야하는 페이로드의 크기를 알려주는 헤더를 
해석해서, 페이로드의 크기를 알아내는 단계야.

그리고 이 시점에서는
상태 관리 구조체(flags)는 정상적으로 진행되었다면 모든 값이 false이고,
header_buf에는 4바이트의 네트워크 바이트 정렬인 클라이언트에서 보낸 헤더가 담겨있어.

> 요약 : recv() 해야하는 페이로드의 크기를 알기 위해 먼저 헤더를 recv() 한 상태이다.
이제 헤더를 해석한 후 해석한 값에 맞춰서 페이로드를 recv() 하면 된다.

---

### 1-3. 페이로드 recv()

이제 헤더까지 해석해서 클라이언트가 몇 바이트를 보내려는지 알았으니,
이제 진짜 데이터 부분인 페이로드를 recv() 해보자.

```cpp
// 페이로드 recv()
int payload_received = 0;

flags.payload_recv = true;
while (payload_received < host_header) {

	int recv_len = recv(client_sock, buf + payload_received, host_header - payload_received, 0);

	if (recv_len == SOCKET_ERROR) {
		err_display("recv()");
		flags.if_error = true;
		break;
	}
	else if (recv_len == 0) {
		flags.if_client_exit = true;
		break;
	}

	payload_received += recv_len;

	if (payload_received != host_header) {
		std::cout << "페이로드 부분적 수신 : " << recv_len << "바이트 수신됨.\n";
	}
}

if (flags.if_client_exit || flags.if_error) break;

flags.payload_recv = false;

buf[payload_received] = '\0';

std::cout << "송신한 클라이언트 : IP 주소 = " << ntohl(client_addr.sin_addr.s_addr) << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';
std::cout << "받은 바이트 수 : " << payload_received << " 받은 메시지 : " << buf << '\n';
````

솔직히 여기는 헤더 recv()랑 원리가 거의 똑같아.

다만 차이가 있다면,
헤더 recv()는 **고정 크기 4바이트(HEADER_SIZE)**를 다 받을 때까지 반복했다면,
페이로드 recv()는 **헤더를 해석해서 얻어낸 host_header 바이트**를 받을 때까지 반복한다는 점 정도야.

---

```cpp
int payload_received = 0;
```

일단 payload_received는
지금까지 페이로드를 총 몇 바이트 받았는지 기록하는 변수야.

TCP는 바이트스트림 프로토콜이라서,
클라이언트가 한 번에 보냈다고 해도 서버 입장에서는 여러 번에 나눠서 받을 수 있어.
그래서 지금까지 몇 바이트를 받았는지 계속 누적해서 기록해야 해.

여기서는 헤더 때처럼 따로 버퍼를 새로 선언하지는 않았어.
이미 accept() 전에

```cpp
char buf[BUFFER_SIZE];
```

이렇게 전체 페이로드를 받을 버퍼를 만들어놨기 때문이야.

그리고 이제 현재 단계가 페이로드 수신 단계라는 걸 표시하기 위해
상태 관리 구조체의 payload_recv 멤버를 true로 바꿔줘.

```cpp
flags.payload_recv = true;
```

---

이제 실제로 recv()를 호출하는 단계야.

```cpp
while (payload_received < host_header) {

	int recv_len = recv(client_sock, buf + payload_received, host_header - payload_received, 0);

	if (recv_len == SOCKET_ERROR) {
		err_display("recv()");
		flags.if_error = true;
		break;
	}
	else if (recv_len == 0) {
		flags.if_client_exit = true;
		break;
	}

	payload_received += recv_len;

	if (payload_received != host_header) {
		std::cout << "페이로드 부분적 수신 : " << recv_len << "바이트 수신됨.\n";
	}
}
```

while문의 조건은

```cpp
지금까지 받은 페이로드 < 헤더에 적혀있는 전체 페이로드 크기
```

야.

즉, 헤더를 해석해서 알아낸 전체 페이로드 크기만큼 아직 다 받지 못했다면,
계속 recv()를 반복 호출하는거지.

---

이제 recv()의 인수들을 보자.

```cpp
int recv_len = recv(client_sock, buf + payload_received, host_header - payload_received, 0);
```

첫 번째 인자는 client_sock.
즉, 지금 연결된 클라이언트와 통신하는 소켓이야.

두 번째 인자는 받은 데이터를 저장할 버퍼 주소인데,
여기서도 헤더 recv() 때와 같은 방식으로 포인터 연산을 사용했어.

```cpp
buf + payload_received
```

이렇게 하면,
지금까지 받은 바이트 수만큼 뒤로 이동한 위치부터 이어서 저장할 수 있어.

예를 들어 지금까지 5바이트를 받았다면,
다음 recv()는 buf + 5 위치부터 저장하게 되고,
결국 새로 받은 데이터가 기존 데이터 뒤에 자연스럽게 이어붙여지게 돼.

세 번째 인자는 이번 recv()로 받고 싶은 바이트 수야.

```cpp
host_header - payload_received
```

이건
전체 페이로드 크기에서 지금까지 받은 바이트 수를 뺀 값이라고 보면 돼.

예를 들어 host_header가 20이고,
지금까지 8바이트를 받았다면,
앞으로 12바이트를 더 받아야 하니까
20 - 8 = 12가 되는거지.

그리고 여기서도 헤더 recv() 때와 마찬가지로
MSG_WAITALL 같은 플래그는 사용하지 않았어.
데이터가 실제로 여러 번에 나눠져서 들어오는 모습을 직접 확인해보고 싶었거든.

recv()의 반환값은
이번 호출에서 실제로 받은 바이트 수니까,
recv_len이라는 변수에 저장해줘.

---

이제 recv() 호출 이후의 예외 처리와,
지금까지 받은 총 페이로드 크기 갱신이야.

```cpp
if (recv_len == SOCKET_ERROR) {
	err_display("recv()");
	flags.if_error = true;
	break;
}
else if (recv_len == 0) {
	flags.if_client_exit = true;
	break;
}

payload_received += recv_len;

if (payload_received != host_header) {
	std::cout << "페이로드 부분적 수신 : " << recv_len << "바이트 수신됨.\n";
}
```

일단 recv_len == SOCKET_ERROR라면
recv() 실행 중 오류가 발생한거니까,
err_display()로 오류를 출력하고
flags.if_error를 true로 바꾼 후 반복문을 탈출해.

반대로 recv_len == 0이라면
클라이언트가 연결을 정상적으로 종료했다는 뜻이니까,
flags.if_client_exit를 true로 바꾼 후 반복문을 탈출하고.

그 다음에는

```cpp
payload_received += recv_len;
```

이렇게 해서 지금까지 받은 전체 페이로드 바이트 수를 갱신해줘.

이건 그냥

```cpp
지금까지 받은 바이트 수 += 이번 recv()에서 받은 바이트 수
```

라고 생각하면 돼.

그리고 만약 아직 전체 페이로드를 다 받지 못했다면,

```cpp
if (payload_received != host_header) {
	std::cout << "페이로드 부분적 수신 : " << recv_len << "바이트 수신됨.\n";
}
```

이렇게 부분적 수신 메시지도 출력해줄 수 있어.

즉,
이번 recv() 한 번으로는 전체 페이로드를 다 받지 못했고,
아직 더 받아야 한다는 뜻이야.

---

이제 방금의 while문 밖으로 나왔다고 해보자.

그럼 이 코드들이 실행돼.

```cpp
if (flags.if_client_exit || flags.if_error) break;

flags.payload_recv = false;

buf[payload_received] = '\0';

std::cout << "송신한 클라이언트 : IP 주소 = " << ntohl(client_addr.sin_addr.s_addr) << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';
std::cout << "받은 바이트 수 : " << payload_received << " 받은 메시지 : " << buf << '\n';
```

먼저 예외 상황부터 다시 확인해.
만약 recv() 도중에 오류가 발생했거나,
클라이언트가 연결을 종료했다면
여기서 바로 바깥 while문으로 나가게 돼.

반대로 정상적으로 끝났다면,
이제 페이로드 수신 단계가 끝난거니까

```cpp
flags.payload_recv = false;
```

이렇게 상태를 다시 false로 돌려놔.

그 다음에는

```cpp
buf[payload_received] = '\0';
```

이렇게 널 문자를 추가해줘.
지금 buf는 그냥 바이트 버퍼일 뿐인데,
이렇게 끝에 널 문자를 붙여주면
문자열처럼 출력할 수 있게 되거든.

그리고 마지막으로
어떤 클라이언트가,
총 몇 바이트를 보냈고,
그 내용이 무엇인지 출력해.

---

일단 이 단계는
헤더를 해석해서 알아낸 크기만큼
실제 페이로드를 끝까지 recv()하는 단계야.

그리고 이 시점에서 정상적으로 진행되었다면,
상태 관리 구조체(flags)는 다시 전부 false 상태가 되고,
buf에는 클라이언트가 보낸 실제 메시지가 들어있어.

> 요약 : 헤더를 해석해서 알아낸 크기만큼 실제 페이로드를 모두 recv()한 상태이다.
이제 이 데이터를 처리하거나, 다시 send()하는 단계로 넘어가면 된다.

---
