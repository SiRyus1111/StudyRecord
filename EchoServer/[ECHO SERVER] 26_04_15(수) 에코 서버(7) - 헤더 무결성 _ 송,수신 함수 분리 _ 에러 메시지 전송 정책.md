## 0. 서론

이번에는 4.13(월)에 했던 
- 헤더의 무결성 검사
- 송 / 수신 과정 함수 분리
- 에러 메시지 전송 정책 설계

에 대해 설명해보려고 해.

일단 그 당시 작성했던 개발 로그.

### 2026.4.13

- 헤더의 버퍼 크기 초과 예외 처리 추가.
  - 클라이언트에서 메시지를 입력 받고 메시지의 바이트 수를 잰 후
  - 서버에서 클라이언트가 보낸 헤더를 recv() 한 후
  - 클라이언트에서 서버가 보낸 헤더를 recv() 한 후
  - 만약 버퍼 최대 크기인 4096(바이트) 이상이라면 에러로 처리한다. (state.if_header_error = true)
  - 클라이언트에서 메시지를 입력 받고 메시지의 바이트 수를 잰 후에는 바이트 초과 메시지를 출력 후 다시 입력을 받는다.
  - 그리고 헤더를 recv() 한 후에는 바로 break하여 페이로드 수신을 차단한다.
- send_all() / recv_all()로 기존 send() / recv() 로직 분리
  - partial send / recv 처리 로직을 공통 함수로 통합
  - `int send_all(SOCKET sock, flags& state, const char* msg, int len)`
  - `int recv_all(SOCKET sock, flags& state, char* buf, int len)`
  - 반환값 : 
	- 상대가 연결 종료시(recv() == 0 / recv_all() 한정) = 0
	- 전송 과정에서 send() / recv() 가 SOCKET_ERROR를 반환할 시 = SOCKET_ERROR
	- 정상적으로 send() / recv() 루프를 마칠 시 = send() / recv()로 처리한 총 바이트 수
  - 통신 과정에서의 예외 상황은 함수 내에서 flags형 구조체인 state에 기록.
  - 현재 진행하는 과정을 알려주는 state의 멤버는 함수 밖에서 state에 기록해야함.
- 서버 -> 클라이언트 에러 메시지 전송 정책 설계
  - 헤더에 너무 큰 값(>4096)이 들어왔을 때(state.if_header_error == true) - 클라이언트에 메시지 보내고 서버 종료
  - 클라이언트가 종료(state.if_peer_exit == true (recv_all() == 0)) - 클라이언트에 아무런 메시지 보내지 않고 종료(딱히 클라이언트에 서버에서의 연결 종료를 알릴 이유가 없음) 
  - send() / recv()가 SOCKET_ERROR를 반환함(state.if_error == true) - 클라이언트에 아무런 메시지 보내지 않고 종료(SOCKET_ERROR가 반환되었다는건 이미 연결이 깨졌을 가능성이 충분히 있기 때문에)
  - transport error(if_error)와 protocol error(if_header_error)를 구분하여 종료 정책을 다르게 설계
- 서버의 flags 구조체 변수 이름 변경
  - flags -> server_state
- flags 구조체의 멤버를 추가 & 가독성을 위해 기존 멤버의 이름을 변경
  - if_header_error 추가
  - if_server_exit / if_client_exit -> if_peer_exit
  
이 개발 로그를 보면, 딱 방금 말했던 세 가지가 있어.

- 헤더의 버퍼 크기 초과 예외 처리 추가 - 헤더의 무결성 검사
- send_all() / recv_all()로 기존 send() / recv() 로직 분리 - 송 / 수신 과정 함수 분리
- 서버 -> 클라이언트 에러 메시지 전송 정책 설계 - 에러 메시지 전송 정책 설계

이제 첫 번째부터 설명해볼게.

## 1. 헤더의 무결성 검사

일단 송 / 수신지에서 
한 번에 보내고, 받을 수 있는 데이터의 양은
내가 지정한 BUFFER_SIZE, 즉 4096바이트야.

애초에 송 / 수신지의 send() / recv()에서 
송 / 수신할 메시지를 담을 버퍼 크기가 
`4096(BUFFER_SIZE) + 1(출력할 때 '\0' 붙일 용도)`
이렇게 4096바이트가 최대라서,
딱 한 번의 송 / 수신에는 최대 4096바이트의 페이로드(데이터)까지만 보낼 수 있어.

하지만, 
- 사용자가 4096바이트 이상을 입력했다면?
- 수신지에서 페이로드의 크기를 나타내는 헤더를 해석했는데 4096 이상으로 해석이 되었다면?

확실히 예외 처리를 해주어야겠지.

그래서, 일단 이런 서버 로직에서
![](https://velog.velcdn.com/images/siryus0907/post/c98a5eb8-dfcb-4689-a007-538f795521c9/image.png)

헤더의 무결성을 검사해야하는 곳은,
적어도 세 곳이야.
- 클라이언트에서 사용자에게 입력을 받고 입력 받은 바이트 수만큼 send()하기 전
- 서버에서 클라이언트가 보낸 헤더를 recv()해서 해석한 후
- 클라이언트에서 서버가 보낸 헤더를 recv()해서 해석한 후

왜 서버에서 클라이언트로 send()할 때에는 검사를 하지 않냐고 질문할 수 있는데,
나는
- 애초에 서버에서 send()로 보낼 헤더는 
서버의 recv()가 완전히 끝났을 때 recv()로 받은 페이로드의 총 바이트를 적어서 보내게 되어있음
- 서버가 클라이언트로부터 보낸 데이터를 recv()를 할 수 있는 경우는 헤더가 정상적(<=4096)인 경우, 
그러므로 받은 페이로드의 총 바이트는 4096을 넘을 수 없음
- 오로지 서버 내에서만 처리하기에 
네트워크를 통한 송 / 수신과 다르게 중간에서 혹시나 변조가 될 가능성 사실상 없음

이렇게 생각해서, 그 부분에는 예외 처리를 하지 않았어.
물론 그 부분에도 예외 처리를 할 필요가 있을지도 모르겠지만,
일단 지금은 예외 처리는 하지 않은 상태야.

그런데, 다시 생각해보니 예외 처리를 해야할 것 같네.
- 서버에서 클라이언트로 send() 하기 전

에는
- 서버에서 클라이언트가 보낸 헤더를 recv()해서 해석한 후
- 클라이언트에서 서버가 보낸 헤더를 recv()해서 해석한 후

이 때와 같은 예외 처리 로직을 사용할 것 같아.
recv()할 값을 다시 입력받을 수 있는 상태가 아니니까..

그래서, 일단 지금은 방금 말한 세 부분에서
각각 예외처리를 한 상태야.

---

1. 클라이언트가 사용자에게 입력을 받고 입력 받은 바이트 수만큼 send()하기 전
```cpp
std::string user_input;
std::cout << "서버에 보낼 메시지 입력(최대 4096바이트, 서버와 연결 종료 희망시 quit 입력) : ";
std::getline(std::cin, user_input);

if (user_input == "quit") break;

strcpy_s(buf, sizeof(buf), user_input.c_str());

// 입력 받은 바이트 수 세기
uint32_t host_header = user_input.size();
uint32_t net_header = htonl(host_header);

if (host_header > BUFFER_SIZE) {
	std::cout << "헤더의 값이 최대 버퍼 크기인 4096(바이트)을 초과. 다시 메시지를 입력해주세요.\n";
	continue;
}

char header_buf[HEADER_SIZE];

memcpy(header_buf, &net_header, sizeof(header_buf));
```
user_input.size()를 해서 입력 받은 바이트 수를 센 뒤에,
```cpp
if (host_header > BUFFER_SIZE) {
	std::cout << "헤더의 값이 최대 버퍼 크기인 4096(바이트)을 초과. 다시 메시지를 입력해주세요.\n";
	continue;
}
```
이렇게 검사를 해서 
헤더의 크기(사용자가 입력한 메시지의 바이트 수)가 허용된 버퍼 크기(4096)를 초과한다면
송 / 수신이 불가하므로, 
다시 메시지를 입력해달라는 메시지를 출력한 후,
continue로 while문의 시작으로, 
그러니까 다시 입력을 받기 전 상태로 돌아가서 다시 입력을 받아.
내가 일부 발췌한 코드에서는 
```cpp
std::string user_input;
std::cout << "서버에 보낼 메시지 입력(최대 4096바이트, 서버와 연결 종료 희망시 quit 입력) : ";
std::getline(std::cin, user_input);
```
여기가 while문 안의 첫 코드야.
그래서 continue를 실행하면 다시 여기부터 코드가 실행이 돼.

여기서 최적화 포인트가 좀 있는 것 같은데,
이번 while 루프에서 당장 헤더 / 페이로드를 송신할 수 있는지 아닌지 여부는
그저 user_input의 길이가 얼만지, 즉 헤더의 값이 몇인지에 따라 달라지는데,
그래서 그걸 검사하기 전에
```cpp
strcpy_s(buf, sizeof(buf), user_input.c_str());
```
이렇게 user_input을 버퍼에 복사하고,
```cpp
uint32_t net_header = htonl(host_header);
```
이렇게 네트워크 바이트 정렬로 바꾸는 과정을 굳이 실행할 필요가 없어.

```cpp
if (host_header > BUFFER_SIZE) {
	std::cout << "헤더의 값이 최대 버퍼 크기인 4096(바이트)을 초과. 다시 메시지를 입력해주세요.\n";
	continue;
}
```
이렇게 헤더의 값을 검사해서 입력받은 메시지가 전송 가능한지 판단 후, 앞의 두 과정을 실행해도 됐었어.
그리고 애초에 4096바이트 이상 입력을 했다면,
strcpy_s() 함수를 실행했을 때부터 문제가 생길 수 있을 것 같아.

그래서 이 부분은 나중에 수정해야할 것 같고.

---

2. 서버에서 클라이언트가 보낸 헤더를 recv()해서 해석한 후, 
클라이언트에서 서버가 보낸 헤더를 recv()해서 해석한 후

이렇게 두 부분은 하나로 묶어도 될 것 같아.

서버에서 클라이언트가 보낸 헤더를 recv()한 후의 코드
```cpp
// 해더 해석
uint32_t net_header;
memcpy(&net_header, header_buf, HEADER_SIZE);

// 보냈을 때 네트워크 바이트 정렬로 보냈으니까 받았을 때 다시 호스트 바이트 정렬로 변환 + 형식도 uint32_t로 유지
uint32_t host_header = ntohl(net_header);

// 헤더가 버퍼 크기에 맞지 않는 경우 처리(4096 초과)
if (host_header > 4096) {
	server_state.if_header_error = true;
	break;
}
```
클라이언트에서 서버가 보낸 헤더를 recv()한 후의 코드
```cpp
// 헤더 해석
uint32_t received_net_header;
memcpy(&received_net_header, header_buf, HEADER_SIZE);
uint32_t received_host_header = ntohl(received_net_header);

if (received_host_header > 4096) {
	client_state.if_header_error = true;
	break;
}
```

둘 다 구조는 사실상 완전히 같아.
일단 네트워크 바이트 정렬의, 
막 recv()한 헤더를 호스트 바이트 정렬의 uint32_t 형으로 바꿔주고,
헤더를 검사한 후, 
버퍼 최대 크기인 4096을 초과하면 상태 관리 구조체의 if_header_error 플래그를 true로 바꾸고,
break로 송 / 수신을 반복하는 while문을 탈출해.

그럼 이제 송 / 수신이 종료되고,
그 이후에 연결 종료 / 에러 메시지 전송 과정이 시작이 되지.

애초에 헤더가 4096을 넘었다는건,
- 뭔가 오류가 발생해서 송신지에서 헤더의 값에 4096 이상을 적어 보냈다
- 클라이언트에서 보낸 패킷이 변조되었다
- 네트워크를 통한 송 / 수신 과정에 문제가 있어서 잘못된 값이 기입된 채로 수신되었다

이런 경우들인데,
이럴 때는 수신지에서 페이로드를 그대로 받으면 문제가 생길 가능성이 높고,
애초에 수신지에서는 페이로드를 몇 바이트를 받아야 할지도 모르기 때문에
그냥 헤더 에러(protocol Error)를 기록해놓고,
그리고 에러 메시지 전송 정책에 따라 에러 메시지를 송신하고 송 / 수신을 종료(closesocket())하거나,
그냥 송 / 수신 과정을 종료(closesocket())해.

---

## 2. 에러 메시지 전송 정책

표 하나로 정리해보자면, 이거야.

| 상황             | 동작            |
| --------------- | --------------- |
| protocol error(응용 계층 - 헤더 에러)  | 메시지 보내고 종료 |
| transport error(전송 계층 - 송수신 에러) | 메시지 없이 종료  |
| peer exit(상대가 연결 종료 - 정상)       | 메시지 없이 종료  |

셋 다 더이상 통신을 이어서 할 수 없다는 공통점이 있지만,
TCP 관점에서 보면 두 경우로 나눌 수 있어.
- 통신 과정에서 오류가 발생했지만, 계속 통신은 가능한 경우
- 아예 TCP 연결 자체가 깨졌을 가능성이 있어서, 계속 통신을 할 수 없는 경우

둘 다 통신을 더 이상 이어갈 수 없다는 점은 같지만, 
TCP 연결 자체가 아직 살아 있는 경우와 이미 신뢰할 수 없는, 깨졌을 가능성이 있는 경우는 구분해야했어.

---

헤더에 너무 큰 값(>4096)이 들어와서 페이로드를 받을 수 없을 때,
TCP 상태 상으로는 아직 연결이 깨지지는 않았지만, 
단순히 프로토콜 상으로 계속 송 / 수신을 할 수 있진 않아.
그래서 충분히 상대에게 메시지를 보낼 수야 있긴 하지만,
오류가 발생해서 더이상 송 / 수신을 이어가기에는 부적합한 상태라는거지.
그래서, 
이렇게 응용 계층의 protocol error가 발생한 경우에는,
상대에게 에러 메시지를 보내서 프로토콜 헤더 해석 에러(if_header_error)가 발생했다는걸 알려주고,
연결을 종료해.

내 코드에서는, 이렇게 송 / 수신 과정이 끝난 후에 실행되는 코드 부분에 이런 로직을 넣었어.

```cpp
else if (server_state.if_header_error) {
	std::cout << "헤더의 값이 4096을 초과. 페이로드 수신 불가.\n";

	uint32_t net_header_err_msg_len = htonl(static_cast<uint32_t>(header_err_msg_szt));
	char err_header_buf[HEADER_SIZE];

	memcpy(err_header_buf, &net_header_err_msg_len, HEADER_SIZE);

	int header_err_send_res = send_all(client_sock, server_state, err_header_buf, HEADER_SIZE);
	if (header_err_send_res == SOCKET_ERROR) {
		std::cout << "헤더 오류 메시지 클라이언트에 전송 실패.\n";
	}
	else {
		int err_send_res = send_all(client_sock, server_state, header_err_msg, host_err_msg_len);
		if (err_send_res == SOCKET_ERROR) {
			std::cout << "헤더 오류 메시지 클라이언트에 전송 실패.\n";
		}
	}
}

// 다른 에러 / 상태 판별 후 closesocket()
```
헤더 오류 메시지를 미리 전역에다 설정해놓고,
프로토콜 헤더 해석 에러(if_header_error)가 발생한 경우,
해당 오류 메시지의 바이트 수를 네트워크 바이트 정렬로 보내서 먼저 송신하고,
그 후에 해당 오류 메시지를 바이트 열로 송신하는 로직이야.
물론 이 과정에서도 오류 메시지의 전송을 실패할 수도 있지만,
이게 연결을 종료할 때의 필수적인 과정은 아니라서
딱히 재전송을 한다거나 하지는 않아.
그리고 수신지에서는 헤더를 받아야 페이로드를 정상적으로 받을 수 있기 때문에,
먼저 헤더를 보내고 잘 보내졌으면(send() == HEADER_SIZE) 
페이로드를 보내는 식으로 로직을 짰어.

---

그리고 소켓 함수가 SOCKET_ERROR를 반환(if_error)해서,
전송 과정에서 에러가 발생했을 때,
TCP 상태 상으로도, 전송 계층에서 연결이 깨졌을 가능성이 있어.
100% 연결이 깨졌다고 장담할 수는 없지만, 그렇다고 소켓 함수가 SOCKET_ERROR를 반환한 상황에서,
그걸 판별하는 로직을 짜는 것보다는 그냥 연결을 종료해버리는게 좋을 것 같았어.

```cpp
// 전역(main() 함수 밖)
const char header_err_msg[] = "[SERVER]헤더의 최댓값 초과됨. 서버에서 연결을 종료합니다.\n";
const std::size_t header_err_msg_szt = sizeof(header_err_msg);
uint32_t host_err_msg_len = static_cast<uint32_t>(header_err_msg_szt);

// 송 / 수신 과정을 종료한 후
if (server_state.if_error) {
	std::cout << "클라이언트와의 통신 과정에서 오류 발생 : ";

	if (server_state.header_recv) std::cout << "헤더 수신 과정에서 오류 발생\n";

	else if (server_state.payload_recv) std::cout << "페이로드 수신 과정에서 오류 발생\n";

	else if (server_state.header_send) std::cout << "헤더 송신 과정에서 오류 발생\n";
	
	else if (server_state.payload_send) std::cout << "페이로드 송신 과정에서 오류 발생\n";


}

// 헤더 에러등 다른 에러, 다른 상태 판별 후 closesocket()
```

이렇게, 따로 에러 메시지를 송신하지 않고,
바로 연결을 종료해.

---

마지막으로, recv()가 0을 반환해서
상대가 closesocket()을 호출해서 정상 종료(Graceful Shutdown)를 했다고 판단할 수 있는 경우에는,
어쨌든 상대가 연결을 종료해서 더이상 통신을 할 수 없기 때문에,
에러 메시지는 보내지 않고 우리도 연결 종료를 해.
```cpp
else if (server_state.if_peer_exit) {
	std::cout << "클라이언트에서 연결을 종료하였습니다.\n";
}

std::cout << "클라이언트와의 연결을 종료합니다. 클라이언트의 IP 주소 = " << addr << " 포트 번호 = " << ntohs(client_addr.sin_port) << '\n';

closesocket(client_sock);
```

---

## 3. 기존 send() / recv() 루프를 함수로 분리

기존의 partial send / recv를 고려한 send() / recv() 루프는
send() / recv()가 필요할 때마다 계속 반복되는 로직이었어.

그리고 TCP는 바이트 스트림 기반 프로토콜이기 때문에, 
한 번의 send()로 원하는 길이의 메시지가 전부 처리된다고 가정할 수 없어서,
이 반복 송신 로직 자체를 함수로 분리할 필요가 있었어.

그래서, 이렇게 아예 함수로 분리했어.

---

```cpp
int send_all(SOCKET sock, flags& state, const char* msg, int len) {
	
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
			std::cout << "부분적 송신 : "<<  send_len << '/' << len << "바이트 송신됨.\n";
		}
	}


	std::cout << "송신 완료 : 총" << sent_byte << '/' << len << "바이트 송신됨.\n";

	return sent_byte;
}
```
send_all() 함수는,
```cpp
int send_all(SOCKET sock, flags& state, const char* msg, int len)
```
이렇게,
- 메시지를 송신할 소켓(sock)
- 송신 중 발생한 에러를 기록할 상태관리 구조체(state)
- 보낼 메시지를 담은 버퍼(msg, 읽기 전용이므로 상수로 선언)
- 보낼 메시지의 길이(len, len만큼만 send()하기 위한 용도)

4가지의 인자를 받아.

그리고 send() 함수가 SOCKET_ERROR를 반환해서 송신 중 에러가 발생한 경우에는
send_all() 함수도 SOCKET_ERROR를 반환하고,

정상적으로 len바이트만큼 송신이 된 경우에는 
send_all() 함수로 처리한 바이트 수(sent_byte)를 반환해.

기존의 send() 함수의 반환값처럼 반환값을 설정해서
send() 함수와 비슷한 느낌으로 사용할 수 있게 반환값을 설계했어.

그리고 이 함수는 기존의 send() 루프 로직을 그대로 따르지.

변경점이라고 한다면,

기존에는 send() 함수 실패 시 루프를 break한 뒤, 
while문 밖에서 상태를 다시 검사해서 처리했어.

하지만 send_all()로 분리한 뒤에는, 
함수 내부에서 바로 SOCKET_ERROR를 반환하도록 해서
send()와 비슷한 방식으로, SOCKET_ERROR를 반환했다는 것만 보고 결과를 처리할 수 있게 했어.

그리고 부분적으로 송신한 바이트 수를 전체 바이트에서 몇 바이트 보냈는지 출력하는 식으로
부분적 송신 / 송신 완료 메시지를 개편했어.
총 몇 바이트 중에 몇 바이트를 송신했는지 출력할 필요가 있다고 생각해서.

그리고 이 함수는 
- 송신 과정에서의 예외 상황은 함수 내에서 flags형 구조체인 state에 기록하지만,
- 현재 진행하는 과정을 알려주는 state의 멤버는 함수 밖에서 state에 기록해야 함

이렇게 규칙을 잡았어.
왜 그렇냐면, 이 함수는 header_send 상태든 payload_send 상태든
범용적으로 사용할 수 있어야 하는데,
함수 내부에서 헤더를 보내는 중인지 페이로드를 보내는 중인지 
추가로 인자를 받아서 검사하는 방식보다는

그냥 함수를 호출하기 전에 상태 관리 구조체의 header_send / payload_send를 true로 설정해서
헤더 / 페이로드를 송신하고 있다고 명시하고,
오류 검사(송신 과정에서 오류가 발생하지 않았는지)까지 끝난 후 
완전히 헤더 / 페이로드 송신이 끝났다면
그 때 header_send / payload_send를 false로 설정하는 방식이 좋아보였어.
```cpp
server_state.header_send = true;
int header_send_res = send_all(client_sock, server_state, header_buf, HEADER_SIZE);

if (header_send_res == SOCKET_ERROR) break;

server_state.header_send = false;
```

즉, send_all() 함수는 송신 자체만 담당하고,
무엇을 송신하고 있는지는 main() 함수에서 따로 관리하게 역할을 분리했어.

물론 송신 과정 중에서 발생한 오류는 함수 내부에서 기록해.
그래서 flags 상태 관리 구조체를 참조 변수로 받는거고.

---

```cpp
int recv_all(SOCKET sock, flags& state, char* buf, int len) {

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

recv_all() 함수도 send_all() 함수랑 대부분이 같아.

recv_all() 함수는,
```cpp
int recv_all(SOCKET sock, flags& state, char* buf, int len)
```
이렇게,
- 메시지를 수신할 소켓(sock)
- 수신 중 발생한 에러를 기록할 상태관리 구조체(state)
- 받을 메시지를 담을 버퍼(buf, 이 버퍼에 받은 메시지를 써야하므로 상수로 선언하지 않음)
- 보낼 메시지의 길이(len, len만큼만 recv()하기 위한 용도)

4가지의 인자를 받아.

그리고 recv() 함수가 SOCKET_ERROR를 반환해서 수신 중 에러가 발생한 경우에는
recv_all() 함수도 SOCKET_ERROR를 반환하고,

상대가 연결 종료를 한 경우(recv() == 0)에는 0을 반환해.

정상적으로 len바이트만큼 수신이 된 경우에는 
recv_all() 함수로 처리한 바이트 수(sent_byte)를 반환해.

이 함수도 기존의 recv() 함수의 반환값처럼 반환값을 설정해서
recv() 함수와 비슷한 느낌으로 사용할 수 있게 반환값을 설계했어.

그리고 이 함수도 기존의 recv() 루프 로직을 그대로 따르지.

변경점은 send_all() 함수와 같아.

그리고 이 함수도 
- 수신 과정에서의 예외 상황은 함수 내에서 flags형 구조체인 state에 기록하지만,
- 현재 진행하는 과정을 알려주는 state의 멤버는 함수 밖에서 state에 기록해야 함

이렇게 규칙을 잡았어.
```cpp
server_state.header_recv = true;
int header_recv_res = recv_all(client_sock, server_state, header_buf, HEADER_SIZE);

if (header_recv_res == SOCKET_ERROR || header_recv_res == 0) break;


server_state.header_recv = false;
```
그래서 이렇게 사용해야해.

물론 이 함수도 수신 과정 중에서 발생한 오류는 함수 내부에서 기록해.
그래서 이 함수에서도 flags 상태 관리 구조체를 참조 변수로 받는거고.