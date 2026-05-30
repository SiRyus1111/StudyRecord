## 0. 서론

이제 내가 지금까지 짠 main() 함수 내부의 코드들을 설명해보려고 해.

근데 여기는 당연하겠지만, 아직 미완성인 부분도 있어서, 
미완성인 부분은 확실히 미완성이라고 적어놓을게.

지금 완성된 부분(물론 나중에 수정 포인트는 있을 수 있음)은
- 헤더 / 라이브러리 / 상태 관리 구조체
- 서버의 기본 세팅 - 윈속 초기화 / listen용 소켓 생성 / 소켓에 IP:Port 바인딩 / listen()
- 송수신 - 완전한 연결 수립(accept()) / 클라이언트로부터 데이터 수신(recv())

이 정도야.

그리고 미완성인 부분은
- 송수신 - 클라이언트에 데이터 송신(send())
- 연결 종료 처리(closesocket())
- 함수 예외처리 이외의 다양한 예외처리
- 클라이언트의 전체 코드

이 정도야.

지금까지 작성한 전체 코드 먼저.
이거 미완성 코드라, 
곧곧에 로직이 빈 부분과 오류가 있을 수 있어.
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
	server_addr.sin_port = htons(9000);
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

뭐.. 첫 프로젝트라 코드의 질은 썩 좋아보이지는 않을 것 같아.
그래도 내가 왜 이 코드를 적었는지는 확실히 설명할 수 있으니,
이정도면 첫 프로젝트로는 OK라고 생각해.
그리고 이 프로젝트가 끝나면,
내가 쓴 코드들을 좀 정리해보려고.
그러니까 리팩토링을 해본다는거야.
Clean Code같은거 참고해서.

## 1. 기본적인 서버 세팅

### 1-1. 윈속 초기화
일단 윈속 라이브러리를 사용하려면 무조건 윈속 초기화를 해줘야해.
윈속 초기화는 `WSAStartup()` 으로 할 수 있는데,
내 코드는 이렇게 있어.
```cpp
WSADATA wsa;

if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
	std::cout << "윈속 초기화 실패\n";
	return 1;
}
```
MAKEWORD(2, 2)는 쉽게 말해서 각 원소들을 1바이트로 보고 2바이트짜리 WORD를 만들어줘.
그래서 이게, 우리는 윈속 2.2버전을 사용할 예정이니, 
MAKEWORD(2, 2)로 2.2버전을 직접 지정한거야.
그리고 중요한게, 이건 거꾸로 읽어야해. 첫 번째 원소가 더 작은 바이트를 나타내서,
만약 MAKEWORD(2, 3)쓰면 실제로는 0x0302 처럼 거꾸로 바뀌어.
ㅋㅋ 왜 이렇게 헷갈리게 만들어놨나 싶다..
근데 그냥 WORD 형(2바이트)의 2진수(16진수)가 들어가면 되니까,
0x0202를 넣어도 돼.
하지만, 나는 0x0202보단 MAKEWORD(2, 2)가 어떤 버전을 쓰는지에 대한 가독성이 더 높아보여서
MAKEWORD()를 썼어.

WSADATA형의 wsa는 그저 이렇게 초기화된 윈속의 정보를 담는 구조체 변수일 뿐이고, 
실제로 해당 구조체를 참조할 일은 많지 않아.
그래도 WSAStartup() 에는 꼭 필요해서 넣었어.
WSAStartup() 함수가 성공적으로 실행되었으면,
WSADATA형의 wsa에는 시스템이 지원하는 최상위 Winsock 버전, 구현 설명 등의 세부 정보가 이 구조체 변수에 채워져.

그리고 이 함수는 정상적으로 실행된다면 0을 반환하는데, 
그렇지 않다면 정상적으로 실행되지 않은 것으로 판단할 수 있어.
그래서 저렇게 윈속 초기화 실패 메시지를 띄우고 프로세스를 종료하지.

WSAStartup() 함수는 서버가 돌아가는 기반의 기반(윈속의 기반)이라서
이 함수가 실행에 실패했다면, 아예 서버 자체가 만들어질 수 없어.
그래서 그냥 프로세스를 종료해버리고, 다시 서버를 키게 하는거야.

그리고 왜 굳이 WSAGetLastError()로 에러 코드를 안 얻냐면,
애초에 WSAStartup()이 실행되어야 WSAGetLastError()가 정상적으로 실행될 수 있기 때문이야.
이게 진짜 윈속의 기반을 까는 함수다보니까,
이 함수가 실행되어야 WSAGetLastError()가 정상적으로 실행될 수 있을거라는 보장이 돼.
그래서 그냥 WSAStartup()의 반환 값만 보고도 에러 코드를 확인할 수 있어.
근데 나는 여기서는 에러 코드는 안 보고 윈속 초기화 실패를 하기만 하면
그냥 main() 함수를 종료하는 식으로 썼어.
나도 이건 왜 이렇게 했는지 모르겠네..
오류코드 신경 안쓰고 일단 오류만 잡으려고 했던 흔적으로 보여.

여긴 수정해야겠다. 이렇게.
```cpp
WSADATA wsa;
int WSAStartup_result = WSAStartup(MAKEWORD(2, 2), &wsa);

if (WSAStartup_result != 0) {
	std::cout << "윈속 초기화 실패. 에러 코드 : " << WSAStartup_result << '\n';
	return 1;
}
```
(실제로 수정함)

---

일단 이 단계는 윈속 API들을 쓸 수 있게 윈속을 초기화하는 단계야.
아직은 소켓은 하나도 생성되지 않았어.

> 요약 : 윈속 API를 쓸 준비만 끝난 상태이다. 아직 소켓은 생성되지 않았다.

---

### 1-2. Listen용 소켓 생성

이제, listen() 함수에 매개변수로 넣을,
LISTENING 상태가 되어서 서버의 대문 역할을 할
소켓을 생성했어.
```cpp
SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // LISTEN용 소켓

if (server_sock == INVALID_SOCKET) {
	err_quit("socket()");
	return 1;
}
```
일단 socket() 함수에,
- IPv4 주소체계를 사용하므로 AF_INET,
- 바이트스트림 타입의 소켓을 사용하므로 SOCK_STREAM,
- 솔직히 여기에 TCP 명시를 안하고 0만 넣어도 TCP로 실행되긴 하는데 
TCP를 사용한다는걸 확실히 명시하기 위해서 IPPROTO_TCP

이렇게 매개변수를 넣었어.

그리고 socket() 함수는 실패하면 INVALID_SOCKET을 반환하니까,
그렇게 반환 값이 나왔을 경우에는 예외처리를 해줘.
이제 WSAGetLastError()를 자유롭게 사용할 수 있으니까,
WSAGetLastError()를 사용하는 err_quit() 함수를 사용해서 에러를 표시해.
그리고 여기서 에러로 인해서 main() 함수를 종료한다는걸 명시하기위해 return 1;까지 적었어.

음.. 그리고 여기랑 다음의 bind() / listen() 까지는 서버의 기반을 세우고 있는거라,
이 함수들이 실행이 안되면 서버 자체가 안 돌아가.
그래서 그냥 프로세스를 종료해버리는 식으로 오류에 대처해.

---

일단 이 단계는 운영체제에게 TCP 소켓 하나를 생성해달라고 요청하는 단계야.
아직은 이 소켓이 어떤 주소를 사용할지도 정해지지 않았고,
연결을 받을 준비가 끝난 것도 아니야.

> 요약: 소켓만 생성된 상태이며, 아직 주소도 없고 연결 대기도 불가능하다.

---

### 1-3. Listen용 소켓과 바인딩할 IP:Port를 담은 소켓 주소 구조체 + 바인딩

이제 확실히 listen용 소켓에 통신을 할 IP:Port를 할당해줘야겠지.
그건 소켓 주소 구조체를 이용해서 할당해줄 수 있어.
그리고 이렇게 소켓에 IP:Port를 할당하는 것을 바인딩이라고 해.
```cpp
sockaddr_in server_addr{}; // LISTEN용 소켓의 소켓 주소 구조체

// 서버 주소 정보 설정
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(9000);
server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
```
일단 IPv4 주소체계를 사용하니, sockaddr_in 구조체를 사용해.
구조체 변수 이름은 서버에서 통신을 할 주소라는 점에서 server_addr이라고 이름을 지었어.
그리고 혹시 쓰레기 값이 들어가지 않도록 구조체 전체를 0으로 초기화했어.
{}은 해당 구조체의 모든 멤버를 0으로 초기화한다는 문법이야.

그리고 구조체 변수의 멤버들을
- sin_family(주소 체계) : AF_INET (IPv4)
- sin_port(통신할 포트 번호) : 9000 (일반적인 테스트용 포트, 네트워크 바이트 정렬)
- sin_addr.s_addr(통신할 IP 주소) : INADDR_ANY (모든 IPv4 주소와 통신, 네트워크 바이트 정렬)

이렇게 초기화해줘.

왜 포트 번호와 IP 주소 멤버는 네트워크 바이트 정렬로 변환하나 싶을텐데,
결국 이 두 값들은 네트워크 밖으로 나가서 상대 호스트가 읽어야하기 때문이야.

네트워크 밖으로 나간다 = 네트워크 바이트 정렬로 변환
그리고 상대 호스트에서 다시 그쪽 호스트의 호스트 바이트 정렬로 바꿔주는거야.
근데 그냥 호스트 바이트 정렬로 보내면,
만약 나와 상대의 바이트 정렬이 다를 경우에 상대 호스트가 해석을 못하겠지.
아예 바이트를 읽는 방향이 다를테니까.

그래서 이 두 가지 멤버는 네트워크 바이트 정렬로 저장하는거야.
<br>
이제 이 소켓 주소 구조체를 이용해서 아까 만들어놨던 Listen용 소켓에 바인딩을 해.
```cpp
// LISTEN용 소켓에 서버 주소 정보 바인딩 
if (bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
	err_quit("bind()");
	return 1;
}
```
일단 바인딩할 소켓은 server_sock(listen용 소켓)이니 첫 번째 인수는 server_sock을 넘겨.

그리고 바인딩할 IP:Port는 server_addr이야. 
그런데 두 번째 인수는 sockaddr_in 구조체가 아닌 
소켓 주소 구조체의 표준인 sockaddr의 포인터(주소값)를 받아.
그래서 server_addr의 주소값을 &연산자를 이용해서 넘기고, 
sockaddr* 형으로 형 변환(캐스팅)해서 넘겨줘.

그리고 마지막은 소켓 주소 구조체의 크기를 넣는,
그러니까 이 함수가 포인터(주소값)로 받은 소켓 주소 구조체를 읽을 때
몇 바이트만큼 읽어야하는지 나타내는 값인데,
그래서 그냥 sizeof(server_addr)로 간단하게 소켓 주소 구조체의 크기를 넘겼어.

그리고 만약 bind() 함수가 SOCKET_ERROR를 반환한다면 
해당 함수에서 오류가 발생했다는거니까
err_quit()을 호출하고 main() 함수를 종료해.

---

일단 이 단계는 서버의 listen용 소켓에 IP:Port를 할당하는 단계야.
이 소켓은 어떤 주소를 사용할지는 정해졌지만,
연결을 받을 준비는 끝나지 않았어.

> 요약 : 생성된 소켓에 주소만 할당된 상태이며, 연결 대기는 불가능하다.

---

### 1-4. TCP 상태를 LISTEN으로 변경 - 클라이언트가 접속 요청 가능하도록

이제 클라이언트가 해당 서버의 IP:Port에 접속할 수 있게 
해당 IP:Port와 연결되어있는 소켓의 TCP 상태를
LISTEN(LISTENING)으로 바꿔줘야겠지.

그러면 클라이언트에서 connect()를 서버의 IP:Port로 호출했을 때,
제대로 3-way handshake를 수행할 수 있을테니까.

그러니까 쉽게 요약하면 서버의 대문을 여는거야.
```cpp
// LISTEN용 소켓을 LISTEN 상태로 전환, 백 로그 크기는 가능한 최대 크기인 SOMAXCONN으로 설정
if (listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
	err_quit("listen()");
	return 1;
}
```
일단 LISTEN 상태로 바꿀 소켓은 앞에서 대놓고 Listen용 소켓이라고 했던 server_sock이므로
첫 번째 인수로 server_sock을 넘겨줘.

그리고, 연결 정보를 담을 큐, 즉 백 로그 큐의 크기는
지금 당장은 어떻게 잡아야할지 모르겠어서
일단 임시로 가능한 최대 크기인 SOMAXCONN으로 해놨어.
백 로그 큐에 대해 조금 더 잘 알아보고 제대로 크기를 잡아볼 생각이야.

그리고 만약 listen() 함수가 SOCKET_ERROR를 반환한다면
해당 함수에서 오류가 발생했다는거니까
err_quit()을 호출하고 main() 함수를 종료해.

---

일단 이 단계는 서버의 listen용 소켓에 주소도 할당했고,
연결을 받을 준비도 끝마친 상태야.

> 요약 : 생성된 소켓에 주소가 할당되었고 해당 소켓은 연결 대기를 하고있다.

---

### 1-5. 본격적인 송수신

일단 여기에 지금까지 짠 전체적인 송수신 코드를 붙일게.
여기가 제일 중요한 파트라 이렇게 하는거야.
```cpp
// 클라이언트와 직접적으로 통신할 소켓과 소켓 주소 구조체
SOCKET client_sock;
sockaddr_in client_addr{};

// 클라이언트로부터 수신한 메시지를 저장할 버퍼
char buf[BUFFER_SIZE + 1];
int addr_len;


// 클라이언트의 접속을 기다리고, 접속이 이루어지면 메시지를 수신한 후 다시 클라이언트로 송신하는 에코 서버
while (true) {

	addr_len = sizeof(client_addr);

	// accept 함수를 실행. accept 함수는 블로킹임. 그래서 연결 정보가 백로그 큐에 들어올 때까지 이 부분에서 멈춰있음. 이 함수를 지나쳐갈까 걱정할 필요 X
	client_sock = accept(server_sock, (sockaddr*)&client_addr, &addr_len);

	if (client_sock == INVALID_SOCKET) {
		err_display("accept()");
		break;
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
```

### 1-5-1. 송수신 준비

```cpp
// 클라이언트와 직접적으로 통신할 소켓과 소켓 주소 구조체
SOCKET client_sock;
sockaddr_in client_addr{};

// 클라이언트로부터 수신한 메시지를 저장할 버퍼
char buf[BUFFER_SIZE + 1];
int addr_len;
```
일단 이렇게 클라이언트와 통신할 준비를 해.
- 클라이언트와 직접적으로 통신할 소켓.
- 클라이언트와 통신할 소켓의 주소를 담을 소켓 주소 구조체.
- 클라이언트로부터 수신한 메시지를 저장할 버퍼.
- 소켓 주소 구조체의 크기를 저장할 변수. accept() 함수에 사용됨.

이렇게 네 가지의 변수 / 버퍼를 만들어놓았어.

일단 accept()의 반환값이 사실상 클라이언트와 통신할 수 있는 소켓이니까,
해당 소켓을 저장할 변수를 만들었어.

그리고 accept() 함수에서 클라이언트의 주소를 저장할 소켓 주소 구조체도 인수로 들어가서,
클라이언트와 통신할 소켓의 주소를 담을 소켓 주소 구조체 변수를 만들었어.

그리고 당연히 수신할 메시지를 저장할 공간,
즉 버퍼도 만들었고.

그리고 accept() 함수는 바로 sizeof()으로 값을 넘기는게 아니라,
실제로 길이가 저장되어있는 포인터(주소)의 값을 넘겨야해서,
따로 소켓 주소 구조체의 크기를 저장할 변수를 추가적으로 만들었어.

그런데, 저기 버퍼 크기 보면, 확실히 이전 코드의 잔재가 남아있는 것 같아.
그 때는 그저 최대 전송 크기 고려안하고 무작정 코드를 짰어서
`BUFFER_SIZE + 1`바이트만큼 버퍼의 크기를 잡아놨었어.
마지막에 수신지에서 이 바이트열의 끝이 어딘지 파악하기 쉽도록
NULL문자(`\0`)을 붙이려했거든.

하지만 우리는 결국 `BUFFER_SIZE`까지만 전송할 예정이니까,
저 `+ 1`은 빼야할 것 같네.
그리고 입력은 `BUFFER_SIZE - 1`까지만 받고,
맨 끝에 NULL문자(`\0`)를 붙이는 식으로 해야겠어.

그리고,
```
// 클라이언트와 직접적으로 통신할 소켓과 소켓 주소 구조체
SOCKET client_sock;
sockaddr_in client_addr{};

// 클라이언트로부터 수신한 메시지를 저장할 버퍼
// 버퍼의 마지막 바이트는 문자열이 끝나는 지점을 나타내는 널 문자('\0')를 저장.
char buf[BUFFER_SIZE];
int addr_len;
```

이렇게 수정했어.

### 1-5-2. accept(), 클라이언트 접속 정보 출력

이제 첫번째 while문의 내부야.

```cpp
// 클라이언트의 접속을 기다리고, 접속이 이루어지면 메시지를 수신한 후 다시 클라이언트로 송신하는 에코 서버
while (true) {

	addr_len = sizeof(client_addr);

	// accept 함수를 실행. accept 함수는 블로킹임. 그래서 연결 정보가 백로그 큐에 들어올 때까지 이 부분에서 멈춰있음. 이 함수를 지나쳐갈까 걱정할 필요 X
	client_sock = accept(server_sock, (sockaddr*)&client_addr, &addr_len);

	if (client_sock == INVALID_SOCKET) {
		err_display("accept()");
		break;
	}

	// 출력용으로 클라이언트 IP 주소를 문자열로 저장
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &client_addr.sin_addr, addr, sizeof(addr));

	std::cout << "클라이언트 접속됨 : IP 주소 = " << addr << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';

	struct flags flags;

    // 헤더 recv 코드
    // 페이로드 recv 코드
    
    // 헤더 send 코드
    // 페이로드 send 코드

}
```
일단, 클라이언트 주소를 저장할 소켓 주소 구조체의 크기를 addr_len 변수에 저장했어.
왜 그렇냐면, 전에 말했듯이 accept() 함수는 소켓 주소 구조체의 포인트(주소)를 받기 때문이야.

그리고 이제 accept()를 실행하지.

일단 첫 번째 인수는 당연히 연결을 받을 소켓, 
listen용 소켓인 server_sock을 넘겨줘.

두 번째 인수는 연결된 클라이언트의 주소 정보를 저장할 소켓 주소 구조체(clinet_addr)를
sockaddr* 형으로 형 변환(캐스팅)해서 넘겨줘.
물론 주소를 넘겨주는거니 앞에 &연산자도 꼭 붙여줘야해.

세 번째 인수는 방금 갱신한 addr_len 변수를 넣어줘.
물론 포인터(주소)가 들어가야하니, 앞에 &연산자를 붙여서.

그리고 이제 오류 처리.
accept() 함수는 실패하면 INVALID_SOCKET을 반환해.
결국 socket() 함수처럼 소켓(소켓 핸들)을 생성하는 함수니까.
그런데 이 함수는 실패하더라도, 충분히 재시도해도 되는 함수야.

전의 WSAStartup() / socket() / bind() / listen() 은
서버의 기반을 깔아주는 함수들이기 때문에,
이 함수들이 실패하면 그냥 프로그램을 다시 실행하는게 좋았어.

하지만 accept() 함수는, 
이미 다 깔린 기반 위에서 새로 연결을 받아오는,
계속 반복할 수 있는 함수이기 때문에
실패해도 다시 시도하면 돼.

그런데 이런 논리로는, 기존 코드에서는 break(while문 탈출)해버리기 때문에,
코드가 그냥 바로 서버도 연결 종료되어버리는 식으로 짜여있었어.

그래서 여기는 수정이 필요해보이네.
다시 accept()를 시도할 수 있게.
```cpp
// 클라이언트의 접속을 기다리고, 접속이 이루어지면 메시지를 수신한 후 다시 클라이언트로 송신하는 에코 서버
while (true) {

	addr_len = sizeof(client_addr);

	// accept 함수를 실행. accept 함수는 블로킹임. 그래서 연결 정보가 백로그 큐에 들어올 때까지 이 부분에서 멈춰있음. 이 함수를 지나쳐갈까 걱정할 필요 X
	client_sock = accept(server_sock, (sockaddr*)&client_addr, &addr_len);

	if (client_sock == INVALID_SOCKET) {
		err_display("accept()");
		continue; // accept() 실패했을 때는 클라이언트와의 연결이 이루어지지 않은 상태이므로, 다음 반복으로 넘어가서 다시 accept() 시도
	}

    // 나머지 코드
}
```
이런 식으로.

그리고 여기까지 코드가 실행되면,
server_sock은 계속 LISTEN 상태로 계속 새로운 연결을 기다리고 있을테고,
client_sock은 클라이언트와 송 / 수신을 할 수 있는 상태가 되겠지.

---

일단 이 단계는 서버의 listen용 소켓으로 받은 클라이언트로부터의 연결 정보를 기반으로
클라이언트와 통신할 새 소켓을 생성한 상태야.

> 요약 : socket() 으로 생성했던 소켓으로 연결 정보를 받아서 클라이언트와 통신할 소켓을 생성했다. 
이제 송 / 수신을 할 수 있다.

---

<br>
이제 클라이언트의 IP:Port를 출력하는 부분이야.
난 어떤 IP의 클라이언트가 해당 클라이언트 내의 어떤 포트로 서버의 접속했는지 출력해보고싶어서
이렇게 해봤어. 
뭔가 실무적으로 생각하면 접속 로그 찍는 느낌이랄까.

일단, 소켓 주소 구조체의 
`sin_addr.s_addr`
즉, `in_addr` 형의 구조체는 바이너리 형태의, 사람이 보면 의미없는 숫자로 보여서,
이걸 문자열로 전환해줘야해.

그건, 전에 인클루드해놨던 ws2tcpip.h 헤더의 inet_ntop() 함수로 할 수 있어.
```cpp
// 출력용으로 클라이언트 IP 주소를 문자열로 저장
char addr[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &client_addr.sin_addr, addr, sizeof(addr));

std::cout << "클라이언트 접속됨 : IP 주소 = " << addr << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';
```
이렇게.
일단 문자열로 변환된 IP 주소를 저장할 C 형식의 문자열(char*)의 버퍼(addr)를 만들어줘.
addr의 길이는, IPv4의 문자열 주소 길이를 나타내는 INET_ADDRSTRLEN만큼으로 해줘.

그리고 inet_ntop()을 실행해.

일단 IPv4의 바이너리 형태의 주소를 변환하는거니까, 
첫 번째 인수로는 IPv4를 나타내는 AF_INET을 넘겨줘.

그리고 두 번째 인수에는 변환할 바이너리 형태의 소켓 주소 구조체의 포인터(1in_addr* 형)를 넘겨줘.
accept()로 생성한 client_sock의, 소켓 주소 구조체인 sin_addr의 주소값을 넘겨주는거야.
여기도 꼭 &연산자로 주소를 넘겨줘야해.

그리고 세 번째 인수에는 문자열로 변환된 IP 주소를 저장할 char* 형의 버퍼를 넘겨줘.
전에 그 용도로 만든 addr을 넘겨주는거야.

네 번째 인수로는 IP 주소를 저장할 char*형의 버퍼의 크기를 넘겨줘.
나는 간단하게 sizeof(addr)을 넘겨줬어.

그리고 마지막,
상태 관리 구조체를 생성해.
```cpp
while (true) {

    // accept() 쪽 코드

    struct flags flags;
    
    // 송 / 수신 코드
}
```

왜 이 시점에 생성하냐면,
accept() 가 성공적으로 실행되었다면 클라이언트의 연결 정보를 출력한 다음에 
바로 이 줄이 실행되게되는데,
적어도 서버와 클라이언트간의 연결이 확실히 수립된 시점 직후에 
송 / 수신 상태를 관리하는 구조체를 생성하는게 제일 좋다고 생각했어.
확실히 연결이 되었고, 
곧 송 / 수신 작업이 시작되니까.

그리고 이 서버를 멀티스레드로 확장하게된다면,
> 1스레드 = 1상태관리구조체

니까,
```
accept() -> accept()로 생성된 소켓을 스레드로 넘기기 -> 스레드 안에서 상태 관리 구조체 생성
```
이런 절차로 갈 것 같아.

이제 송 / 수신 준비 완전히 끝!