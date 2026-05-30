## 0. 서론

일단 최근에는 너무 블로그에 자주 기록하는건 안좋아보여서 글 빈도를 좀 조절하고있어.

하지만, 이번에는 
- 사실상 기본 뼈대가 되는 코드들은 전부 완성했고, 
- 빌드 / 런타임 에러도 잡아봤고,
- 실행도 해봤고 정상 작동도 확인해서,

한번 글을 써봤어.

일단 이번 글은 클라이언트의 전체 구현에 관한 글이고,
디버깅 + 실행은 다음 글에 쓸게.

[깃허브 링크](https://github.com/SiRyus1111/My-First-Echo-Server-Project)

---

## 1. 클라이언트의 전체 구조

이건 첫 글에서 썼던 구조와 거의 달라지지 않았어.
![](https://velog.velcdn.com/images/siryus0907/post/6efb96d2-fb9d-43f6-b4e3-15dcc40e9a5a/image.png)
![](https://velog.velcdn.com/images/siryus0907/post/c1718a88-0026-425b-bd09-ba1d45940787/image.png)
![](https://velog.velcdn.com/images/siryus0907/post/5adcd49b-4f03-41f2-9947-07645882af25/image.png)



```cpp
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Common.h"
#include <cstring>
#include <string>

const char* SERVER_ADDR = "127.0.0.1";
const int SERVER_PORT = 9000;
const int BUFFER_SIZE = 4096;
const int HEADER_SIZE = 4;

struct flags {
	bool header_recv = false;
	bool payload_recv = false;
	bool header_send = false;
	bool payload_send = false;

	bool if_error = false;
	bool if_server_exit = false;
};

// 헤더 규칙
// 첫 4바이트 = uint32_t 페이로드 크기(길이)

int main() {

	WSADATA wsa;
	int WSAStartup_result = WSAStartup(MAKEWORD(2, 2), &wsa);
	
	if (WSAStartup_result != 0) {
		std::cout << "윈속 초기화 실패. 에러 코드 : " << WSAStartup_result << '\n';
		return 1;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 서버와 통신할 소켓

	if (sock == INVALID_SOCKET) {
		err_quit("socket()");
		return 1;
	}

	char buf[BUFFER_SIZE + 1];

	sockaddr_in server_addr{};

	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
	server_addr.sin_port = htons(SERVER_PORT);

	if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		err_quit("connect");
		return 1;
	}

	std::cout << "서버에 연결 성공\n";

	flags client_state;
	while (true) {

		std::string user_input;
		std::cout << "서버에 보낼 메시지 입력(최대 4096바이트, 서버와 연결 종료 희망시 quit 입력) : ";
		std::getline(std::cin, user_input);

		if (user_input == "quit") break;

		strcpy_s(buf, sizeof(buf), user_input.c_str());

		// 입력 받은 바이트 수 세기
		uint32_t host_header = user_input.size();
		uint32_t net_header = htonl(host_header);
		
		char header_buf[HEADER_SIZE];

		memcpy(header_buf, &net_header, sizeof(header_buf));

		// 헤더, 페이로드 각각 send()
		int header_sent = 0;

		client_state.header_send = true;
		while (header_sent < HEADER_SIZE) {
			int header_send_len = send(sock, header_buf + header_sent, HEADER_SIZE - header_sent, 0);

			if (header_send_len == SOCKET_ERROR) {
				err_display("send");
				client_state.if_error = true;
				break;
			}

			header_sent += header_send_len;

			if (header_sent != HEADER_SIZE) {
				std::cout << "헤더 부분적 송신 : " << header_send_len << "바이트 송신됨.\n";
			}
		}
		
		if (client_state.if_error) break;

		client_state.header_send = false;

		std::cout << "헤더 송신 완료 : 총" << header_sent << "바이트 송신됨.\n";

		int payload_sent = 0;

		client_state.payload_send = true;
		while (payload_sent < host_header) {
			int payload_send_len = send(sock, buf + payload_sent, host_header - payload_sent, 0);

			if (payload_send_len == SOCKET_ERROR) {
				err_display("send()");
				client_state.if_error = true;
				break;
			}

			payload_sent += payload_send_len;

			if (payload_sent != host_header) {
				std::cout << "페이로드 부분적 송신 : " << payload_send_len << "바이트 송신됨.\n";
			}
		}

		if (client_state.if_error) break;

		client_state.payload_send = false;

		std::cout << "페이로드 송신 완료 : 총 " << payload_sent << "바이트 송신됨.\n";

		// 헤더, 페이로드 각각 recv()
		int header_received = 0;
		client_state.header_recv = true;
		while (header_received < HEADER_SIZE) {
			int header_recv_len = recv(sock, header_buf + header_received, HEADER_SIZE - header_received, 0);

			if (header_recv_len == SOCKET_ERROR) {
				err_display("recv()");
				client_state.if_error = true;
				break;
			}
			if (header_recv_len == 0) {
				client_state.if_server_exit = true;
				break;
			}

			header_received += header_recv_len;

			if (header_received != HEADER_SIZE) {
				std::cout << "헤더 부분적 수신 : " << header_recv_len << "바이트 수신됨.\n";
			}
		}
		if (client_state.if_error || client_state.if_server_exit) break;

		client_state.header_recv = false;

		std::cout << "헤더 수신 완료 : 총 " << header_received << "바이트 수신됨. 헤더를 해석합니다.\n";
		
		uint32_t received_net_header;
		memcpy(&received_net_header, header_buf, HEADER_SIZE);
		uint32_t received_host_header = ntohl(received_net_header);

		std::cout << "헤더 해석 완료. 페이로드를 수신합니다.\n";

		int payload_received = 0;

		client_state.payload_recv = true;
		while (payload_received < received_host_header) {
			int payload_recv_len = recv(sock, buf + payload_received, received_host_header - payload_received, 0);

			if (payload_recv_len == SOCKET_ERROR) {
				err_display("recv()");
				client_state.if_error = true;
				break;
			}
			if (payload_recv_len == 0) {
				client_state.if_server_exit = true;
				break;
			}

			payload_received += payload_recv_len;

			if (payload_received != received_host_header) {
				std::cout << "페이로드 부분적 수신 : " << payload_recv_len << "바이트 수신됨.\n";
			}
		}
		if (client_state.if_error || client_state.if_server_exit) break;

		client_state.payload_recv = false;

		buf[received_host_header] = '\0';

		std::cout << "[ECHO FROM SERVER]" << buf << '\n';
	}
	// break시 오류 발생 체크, 클라이언트 종료하기
	// 여기도 조건 많이 추가되면 Branch Prediction 성능 떨어질 듯..
	// 근데 어떻게 해야할지 모르겠다.. 원래 이렇게 해도 되는건가?
	if (client_state.if_error) {
		std::cout << "서버와의 통신 과정에서 오류 발생 : ";

		if (client_state.header_send) std::cout << "헤더 송신 과정에서 오류 발생\n";

		if (client_state.payload_send) std::cout << "페이로드 송신 과정에서 오류 발생\n";

		if (client_state.header_recv) std::cout << "헤더 수신 과정에서 오류 발생\n";

		if (client_state.payload_recv) std::cout << "페이로드 수신 과정에서 오류 발생";
	}
	else if (client_state.if_server_exit) {
		std::cout << "서버에서 연결 종료\n";
	}
    else {
	std::cout << "정상적으로 연결 종료됨..\n";
    }


	closesocket(sock);

	WSACleanup();

	return 0;
}
```

main() 함수의 실행 흐름을 보면,

> 기본 준비(WSAStartup() / socket() / 서버 주소를 저장할 소켓 주소 구조체 생성)
-> 서버에 연결(connect(), 소켓 주소 구조체 사용)
-> 메시지 입력받기(getline()) / 송수신(send() / recv()) - 반복
-> quit을 입력받으면 정상 종료인지 에러인지 어느 부분의 에러인지 출력
-> 종료(closesocket() / WSACleanup())

이렇게 돼.

그리고 전의 서버 코드와 중복되는 부분이 많은데,
- WSAStartup()
- send()
- recv()

이렇게 세 개는 전의 서버 코드와 완전히 동일한 로직이라서
따로 설명할 필요는 없을 것 같아.

---

## 2. 기본 준비

일단 
```cpp
WSADATA wsa;
int WSAStartup_result = WSAStartup(MAKEWORD(2, 2), &wsa);

if (WSAStartup_result != 0) {
	std::cout << "윈속 초기화 실패. 에러 코드 : " << WSAStartup_result << '\n';
	return 1;
}

SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 서버와 통신할 소켓

if (sock == INVALID_SOCKET) {
	err_quit("socket()");
	return 1;
}

char buf[BUFFER_SIZE + 1];

sockaddr_in server_addr{};

server_addr.sin_family = AF_INET;
inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
server_addr.sin_port = htons(SERVER_PORT);
```

이 부분은 서버와 통신하기 위한 기본적인 준비야.

1. 윈속 초기화 
- WSAStartup()
```cpp
WSADATA wsa;
int WSAStartup_result = WSAStartup(MAKEWORD(2, 2), &wsa);

if (WSAStartup_result != 0) {
	std::cout << "윈속 초기화 실패. 에러 코드 : " << WSAStartup_result << '\n';
	return 1;
}
```
2. 서버와 통신할 소켓 생성 
- socket() - IPv4 / 스트림 방식 / TCP
```cpp
SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 서버와 통신할 소켓

if (sock == INVALID_SOCKET) {
	err_quit("socket()");
	return 1;
}
```
3. 송신할 메시지 / 수신한 메시지를 저장할 버퍼 선언 
- 크기는 상수로 선언해놓은 BUFFER_SIZE + 1(출력 시에 메시지 경계를 표현하는데 사용할 널문자(\0)용)
```cpp
char buf[BUFFER_SIZE + 1];
```
4. connect()에 사용할 서버의 IP:Port를 담을 소켓 주소 구조체 생성
- family = IPv4(AF_INET)
- addr = 미리 상수로 선언해놓은 서버 주소(SERVER_ADDR - 로컬 호스트) 바이너리로 변환 
- port = 미리 상수로 선언해놓은 서버 포트(SERVER_PORT) 네트워크 바이트 정렬로 변환
```cpp
sockaddr_in server_addr{};

server_addr.sin_family = AF_INET;
inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
server_addr.sin_port = htons(SERVER_PORT);
```

이 정도.

전에 버퍼 크기를 어떻게 할까 고민을 많이 했는데,
어차피 널문자(\0)는 출력할 때에만 의미가 있다고 생각해서,
메시지를 받은 수신지에서 따로 붙이는 방식을 선택했어.
송신지에서는 최대 4096바이트만 송신하고, 수신지에서 남은 1바이트에 널문자(\0)붙이기.
그래서 버퍼 크기는 상대에게 전송할 / 상대에게서 받을 4096바이트(BUFFER_SIZE) + 널문자용 1바이트(1)
이렇게 정했어.

물론 앞에서도 말했듯이 실제로 송 / 수신할 수 있는 최대 바이트 수는 4096바이트야.
이건 중요해서.
나중에 버퍼 오버플로우가 발생하지 않게 예외처리 할 때?

여긴 딱히 특별한게 없네.

---

## 3. connect() - 서버에 연결

이제 이전에 생성해놓은 소켓 주소 구조체, server_addr을 사용해서 connect()를 호출해.
```cpp
if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
	err_quit("connect");
	return 1;
}

std::cout << "서버에 연결 성공\n";
```
일단 이전에 socket()으로 생성해놓은, 서버와 통신할 sock이라는 소켓을 첫 번째 인수로 넣고,

기존에 생성해놓은 서버의 IP:Port를 담은 sockaddr_in 형의 구조체인 
server_addr을 두 번째 인수와 맞는 sockaddr* 형의 구조체로 형변환을 해서 두 번째 인수로 넣어.

그리고 server_addr 구조체의 크기를 sizeof() 연산자를 사용해서 세 번째 인수로 넣어.

---

## 4. 메시지 입력받기 + 헤더 구현

```cpp
flags client_state;
```
> 메시지 입력받기 -> 송신 -> 수신 -> 출력

으로 이어지는 반복문 전에, 미리 클라이언트의 상태를 기록해둘 flags 상태 관리 구조체인 client_state를 선언해놔.
이건 나중에 종료할 때도 참조해야 하는 구조체라,
스코프가 코드의 끝까지 이어져야 해서,
본격적인 while문에 들어가기 전에 미리 선언해놨어.
```cpp
while (true) {

	std::string user_input;
	std::cout << "서버에 보낼 메시지 입력(최대 4096바이트, 서버와 연결 종료 희망시 quit 입력) : ";
	std::getline(std::cin, user_input);

	if (user_input == "quit") break;

	strcpy_s(buf, sizeof(buf), user_input.c_str());

	// 입력 받은 바이트 수 세기
	uint32_t host_header = user_input.size();
	uint32_t net_header = htonl(host_header);
	
	char header_buf[HEADER_SIZE];

	memcpy(header_buf, &net_header, sizeof(header_buf));

	// 송 / 수신. 에러가 발생했거나 서버가 종료되었으면 client_state 구조체에 명시한 후 break
}
```

일단 나는 입력을 받는데에 getline()을 사용했어.
cin은 구분자로 줄바꿈을 인식하지만 공백도 인식해서 공백으로도 문자열이 끊기지만,
getline()은 공백을 구분자로 인식을 안하고, 오직 줄바꿈으로만 구분자로 인식해서
공백도 포함해서 문자열을 표현하기 위해 getline()을 사용했어.

그리고 getline()은 string 형만 인수로 받으니,
일단 getline()으로 인수를 받을 때는 string 형인 user_input을 인수로 넣지만,
결국 send() 함수로 전송하는건 string 형이 아니라, char[] 형의 바이트열, 그러니까 버퍼(buf)이기 때문에,

strcpy_s() 함수로 user_input의 데이터를 c_str()을 사용해서 버퍼(buf)로 옮겼어.

그리고, 입력 받은 바이트 수를 세서 서버가 몇 바이트를 받아야 하는지 알 수 있도록 
헤더에 문자열의 바이트 수를 명시를 해야하는데,
sizeof()이 아니라 string 라이브러리에 있는 size() 함수를 사용했어.

sizeof()은 쉽게 말하자면 해당 변수의 자료형의 크기를 재는 함수고,
string 형은 문자 수에 따라 크기가 변동되니,
sizeof()을 사용하면 안돼.

그러니까 동적 할당(heap, string), 정적 할당(stack, char[])의 차이라고 볼 수 있지.
string 형은 입력받는 값에 따라 크기가 실행 때 마다 변하니까.

string 라이브러리의 size() 함수는 오직 string 형의 바이트 수를 세는 함수로써 만들어졌으며,
실행 때 마다 변하는 string형 문자열의 크기를 반영할 수 있어서
그래서 나는 size()를 썼어.

그리고 서버에서 다시 서버의 바이트 정렬로 바꿀 수 있게,
htons()을 사용해서 네트워크 바이트 정렬인 빅 엔디언으로 바꿨어.

그리고 이렇게 네트워크 바이트 정렬로 바꾼 바이트열을 
memcpy() 함수를 사용해서 
실제로 송신에서 사용할, 헤더를 저장하는 버퍼인 header_buf에 그대로 바이트열을 복사했어.

---

그런데 send() / recv()는 서버에서의 구현과 로직이 완전히 같아.
부분 송 / 수신을 방지하기 위해 지정된 길이(HEADER_SIZE / host_header / received_host_header)만큼
send() / recv() 할 때까지 계속 send() / recv() 해주고,
중간에 서버에서 종료 / 오류 발생하면 client_state 상태 관리 구조체에 오류를 명시하고.
각 send() / recv() 에 들어갈 때 client_state 상태 관리 구조체에 명시해주고.

그래서 이 부분은 중복으로 설명한다 생각해서 딱히 설명하지는 않을게.
진짜로 로직이 똑같아.
변수 이름만 달라.

---

## 5. 에러 판별

이제 
- 마지막에 exit를 입력해서 송 / 수신 루프를 break했든,
- 에러가 발생해서 송 / 수신 루프를 break했든,
- 서버에서 연결이 끊겨서 송 / 수신 루프를 break했든,

알맞은 출력을 해줘야해.

- exit - 정상 종료 메시지 출력
- 에러 발생 - 에러 발생을 알리는 메시지와 에러가 발생한 과정 출력
- 서버에서 연결끊김 - 서버에서 연결을 종료했다는 메시지 출력

이렇게 구분해서 출력하는게 디버깅하는 관점에서도 이득이겠지.

```cpp
// break시 오류 발생 체크, 클라이언트 종료하기
// 여기도 조건 많이 추가되면 Branch Prediction 성능 떨어질 듯..
// 근데 어떻게 해야할지 모르겠다.. 원래 이렇게 해도 되는건가?
if (client_state.if_error) {
	std::cout << "서버와의 통신 과정에서 오류 발생 : ";

	if (client_state.header_send) std::cout << "헤더 송신 과정에서 오류 발생\n";

	if (client_state.payload_send) std::cout << "페이로드 송신 과정에서 오류 발생\n";

	if (client_state.header_recv) std::cout << "헤더 수신 과정에서 오류 발생\n";

	if (client_state.payload_recv) std::cout << "페이로드 수신 과정에서 오류 발생";
}
else if (client_state.if_server_exit) {
	std::cout << "서버에서 연결 종료\n";
}
else {
	std::cout << "정상적으로 연결 종료됨..\n";
}

closesocket(sock);

WSACleanup();

return 0;
```
그래서, 일단 에러가 발생했는지 확인한 후(client_state.if_error == true),
어느 파트에서 오류가 발생했는지 진행 상태(과정)을 나타내는 플래그를 보고 알맞은 상태를 출력하고,

에러가 발생하지 않았으면 서버에서 연결을 종료했는지 확인한 후(client_state.if_server_exit),
서버에서 연결을 종료했다면 서버에서 연결을 종료했다는 메시지를 출력해.

그리고 둘 다 아니라면 정상적으로 연결이 종료되었다는 뜻, exit를 입력해서 연결을 종료했다는 뜻이므로,
정상적 연결 종료 메시지를 출력해.

그리고 closesocket()을 호출해서 4-way handshake를 진행하고,
WSACleanup()으로 윈속을 종료해.

그리고 이 파트에서 주석에 쓴 것처럼 Branch Prediction(분기 예측, CPU의 파이프라인이 flush되는 문제) 문제가 발생하지 않을까 싶었는데,
실제로 이렇게 단순히 한 번만 실행되는 코드에서는 이런 문제는 크게 성능에 문제를 끼치지 않는다고 하네.

진짜로 문제를 끼치는건 수천, 수만번 실행되는 반복문 안에서 Branch Prediction이 진짜로 문제가 된다고.