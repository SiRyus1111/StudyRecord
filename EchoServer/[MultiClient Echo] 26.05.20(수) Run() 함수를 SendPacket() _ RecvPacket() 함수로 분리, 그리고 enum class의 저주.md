## 0. 서론

이번에는 `ClientSession` 클래스의
`Run()`함수를 분리하면서 벌어진 일들을 말해볼게.

일단 왜 `Run()` 함수를 분리했냐면,
기존의 `Run()` 함수는 
에코 서버의 로직에 맞춰 송신부터 수신까지 전부 다 하는 일체형 함수였어.

하지만, 이제 브로드캐스트를 해야하는데,
기존 에코 서버의 로직으로는 무리겠지?

그래서 `ClientSession::Run()` 함수의 송신과 수신을 분리해야했어.

- 송신 담당 - `SendPacket()`
- 수신 담당 - `RecvPacket()`

이렇게.

그리고 추가적으로 `ClientState` 구조체를 보고
발생한 예외를 판정하는
`TransportExceptionHandling()` 함수와,

`closing`을 `true`로 바꿔주는 `MarkClosing()` 함수도 만들었는데,
이건 짧게 다뤄볼게.

근데 사실 이런 것들은 본론이 아니야.
그냥 대부분 코드 복붙이거든.

그래서 초반에 짧게 하고,
본론인 `enum class`의 저주 아닌 저주에 대해서
이야기해볼게.

## 1. `Run()` 함수를 `SendPacket()` / `RecvPacket()` / `MarkClosing()` / `Tra..Handling()` 함수로 분리

앞에서 말했듯이 기존의 `Run()` 함수는 에코 서버 로직이 그대로 박혀있는 일체형 함수였어.
물론 이 프로젝트가 평생 에코서버만 할거라면 괜찮았겠지만,
이 프로젝트는 에코서버 뿐만 아니라 채팅서버까지 해야했어.

즉, 브로드캐스트를 해야했다는거지.

한 클라이언트에서 보낸 패킷을 `recv_all()`하면,
다른 모든 클라이언트에 `send_all()`를 해줘야했어.

`Run()` 함수는 좀 과장해서

```text
송신
|  ^
|  |
|  | -------- 중간에 예외 발생하면 예외처리
v  |
수신
```

진짜 딱 이 구조였어.

송신 한 번에 수신 한번,
`recv_all()` 한 번에 `send_all()` 한 번..
위에서 설명한
> 한 클라이언트에서  `recv_all()`
다른 모든 클라이언트에 `send_all()`

을 근본적으로 할 수 없는 구조야.

그래서, 
`ClientSession` 객체에서 송신 따로, 수신 따로, 예외처리 따로하는 식으로
`Run()` 함수를 분리해야했어.

사실 코드 복붙임..

```cpp
NetState ClientSession::SendPacket(const char* msg, uint32_t len, PacketType type) {
	if (len > PAYLOAD_SIZE) {
		ClientState.if_header_error = true;
		return ClientState;
	}

	PacketHeader send_net_header{};
	send_net_header.length = htonl(len);
	send_net_header.type = htonl(static_cast<int32_t>(type));

	ClientState.header_send = true;
	int header_send_res = ClientSock->ClientSockSend(ClientState, (char*)&send_net_header, sizeof(PacketHeader)); // 해당 함수 내에서 transport error나 peer exit는 기록됨

	if (header_send_res == SOCKET_ERROR) {
		return ClientState;
	}
	ClientState.header_send = false;

	ClientState.payload_send = true;
	int payload_send_res = ClientSock->ClientSockSend(ClientState, msg, len);

	if (payload_send_res == SOCKET_ERROR) {
		return ClientState;
	}
	ClientState.payload_send = false;

	return ClientState;
}
```

```cpp
NetState ClientSession::RecvPacket(char* buf) {

	// 헤더 수신
	PacketHeader recv_net_header{};

	ClientState.header_recv = true;
	int header_recv_res = ClientSock->ClientSockRecv(ClientState, (char*)&recv_net_header, sizeof(PacketHeader));

	if (header_recv_res == SOCKET_ERROR || header_recv_res == 0) {
		return ClientState;
	}
	ClientState.header_recv = false;

	PacketHeader recv_host_header{};
	recv_host_header.type = ntohl(static_cast<int32_t>(recv_net_header.type));
	recv_host_header.length = ntohl(recv_net_header.length);

	if (recv_host_header.length > PAYLOAD_SIZE || recv_host_header.length == 0) { // length == 0이어도 protocol error로 처리
		ClientState.if_header_error = true;
		return ClientState;
	}

	if (recv_host_header.type != static_cast<int32_t>(PacketType::SAFE) &&
		recv_host_header.type != static_cast<int32_t>(PacketType::HEADER_ERROR)) {
		ClientState.if_header_error = true;
		return ClientState;
	}

	// 페이로드 수신
	ClientState.payload_recv = true;
	int payload_recv_res = ClientSock->ClientSockRecv(ClientState, (char*)buf, recv_host_header.length);

	if (payload_recv_res == SOCKET_ERROR || payload_recv_res == 0) {
		return ClientState;
	}
	ClientState.payload_recv = false;

	buf[recv_host_header.length] = '\0';

	std::cout << "송신한 클라이언트 : IP 주소 = " << ClientAddrStr << " 포트 번호 = " << ntohs(ClientAddr.sin_port) << '\n';
	std::cout << "받은 총 바이트 수 : " << payload_recv_res + header_recv_res << " 받은 메시지 : " << buf << '\n';

	if (recv_host_header.type == static_cast<int32_t>(PacketType::HEADER_ERROR)) {
		ClientState.if_peer_error = true;
		return ClientState;
	}

	return ClientState;
}
```
이거 생각보다 기네..

`NetState` 상태 관리 구조체를 반환하는 이유는
각각의 송 / 수신이 끝난 후 상태를 반환하는 목적이야.
만약 송신이 끝났는데 `if_error` 플래그가 `true`다?

그러면 바로 transport error가 났다는걸 알 수 있겠지.

실제 코드에선 이렇게 체크했어.
```cpp
NetState recv_state = RecvPacket(buf);

if (recv_state.if_error ||
	recv_state.if_header_error ||
	recv_state.if_peer_error ||
	recv_state.if_peer_exit) break;
```
예외 상황이 발생하면 `break`.


그리고 여기서 좀 추가된 점이 있다면,

1. `PacketHeader::length == 0`이어도 protocol error로 판별하는 정책이 확립되었다.
2. 따로 문자열 형태의 IP주소를 담는 `ClientAddrStr`이 추가되었다.

이 정도.
`ClientAddrStr`은 이렇게 생성자에서 `inet_ntop()` 함수로 값을 받아줘.
```cpp
private:
char ClientAddrStr[INET_ADDRSTRLEN];

public:
	ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) : 
		ClientSock(std::move(s)), 
    	ClientAddr(addr), 
    	ClientAddrStr{}, 
    	ClientState{}, 
    	closing(false) { // move로 ClientSocket unique_ptr 객체 옮기기, addr 소켓 주소 구조체는 간단한 구조체이므로 단순 복사
    
		inet_ntop(AF_INET, &ClientAddr.sin_addr, ClientAddrStr, sizeof(ClientAddrStr));
    
	}
```
이런 느낌으로.

그리고 기존의 `Run()` 함수에는 송 / 수신이 끝난 후에
종료의 원인을 `ClientState` 보고 판별해서 
원인에 맞는 예외 처리를 해주는 코드가 있었거든?

그 코드는 따로 `TransportExceptionHandling()` 함수로 뺐어.
```cpp
void TransportExceptionHandling() {

	if (ClientState.if_error) {
		std::cout << "클라이언트와의 통신 과정에서 오류 발생 : ";

		if (ClientState.header_recv) std::cout << "헤더 수신 과정에서 오류 발생\n";

		else if (ClientState.payload_recv) std::cout << "페이로드 수신 과정에서 오류 발생\n";

		else if (ClientState.header_send) std::cout << "헤더 송신 과정에서 오류 발생\n";

		else if (ClientState.payload_send) std::cout << "페이로드 송신 과정에서 오류 발생\n";

	}
	else if (ClientState.if_header_error) {

		std::cout << "헤더의 값이 4096을 초과. 페이로드 수신 불가.\n";

		PacketHeader protocol_err_header;
		protocol_err_header.type = htonl(static_cast<int32_t>(PacketType::HEADER_ERROR));
		protocol_err_header.length = htonl(host_err_msg_len);

		int header_err_send_res = ClientSock->ClientSockSend(ClientState, (char*)&protocol_err_header, HEADER_SIZE);
		if (header_err_send_res == SOCKET_ERROR) {
			std::cout << "헤더 오류 메시지 클라이언트에 전송 실패.\n";
		}
		else {
			int err_send_res = ClientSock->ClientSockSend(ClientState, header_err_msg, host_err_msg_len);
			if (err_send_res == SOCKET_ERROR) {
				std::cout << "헤더 오류 메시지 클라이언트에 전송 실패.\n";
			}
		}
	}
	else if (ClientState.if_peer_error) {
		std::cout << "클라이언트에 보낸 헤더의 오류 수신.\n";
	}
	else if (ClientState.if_peer_exit) {
		std::cout << "클라이언트에서 연결을 종료하였습니다.\n";
	}

	return;
}
```

그냥 기존에 송 / 수신 종료 후 처리 코드 복붙이지 뭐..

추가적으로, 따로 `closing`을 `true`로 변경하는, 
`MarkClosing()` 함수도 만들었어.

`closing`은 함부로 건드리기 위험하니까,
항상 `closing`을 건드릴 때는 이 함수를 사용하기로 했어.

## 2. `enum class`와 raw value

전에 객체 표현과 생명 주기 글에 쓴 것처럼,
`enum class`는
> 연관된 상수를 하나로 묶는 것

즉, 상수의 **의미**의 관점에서의 객체야.
해당 상수들이 어떤 의미인지를 까먹지 않게 나열해놓은 객체라고 할 수 있지.

그리고 raw value, 즉 원시 값.
int, float같은 자료형의 값.
우리가 일반적으로 사용하는 값들.

이런 것들은 **의미**보다는 **값 그 자체**에 집중하지.

여기서 정말 중요한 포인트가 있었어.

내 멀티클라 에코서버 프로젝트에는
이전의 싱글클라 에코서버로부터 이어져온 프로토콜이 있었어.
```text
[PacketHeader][Payload]
```
이 구조의.

그리고 `PacketHeader` 구조체에는 `type`라는 멤버가 있었고,
나는 그 `type` 멤버를 단순히 
**의미**를 담는 `PacketType`, 즉 `enum class`로 정의해놨었어.

하지만 `PacketHeader`는 실제로 바이트열 취급되어서,
`htonl()` 함수로 바이트 정렬이 바뀌어서 송신되는 변수라는걸 잊고있었어.
즉, 의미라기보단 **값 그 자체**지.

하지만 그걸 **의미**를 담는 `PacketType` `enum class` 객체로 정의했으니..

`enum class`가 `==`같은 비교 연산자에 엄청 까다롭고,
암시적 형변환으로 대입될 수 없는거 알지?

그리고 `htonl()` 함수의 인자로는 오직 숫자, 즉 **값**만 들어갈 수 있었고.

그 결과는..
그냥 캐스팅 지옥이 펼쳐졌어.

(`PacketType::SAFE`를 `header` 구조체의 `type` 멤버에 넣는 과정..)
```cpp
                                                                 // 그저 PacketType::SAFE
                                                                 PacketType::SAFE
```
```cpp
                                            // htonl() 함수에 집어넣기 위해 정수로 캐스팅
                                            static_cast<int32_t>(PacketType::SAFE)
```
```cpp
								      // htonl()
                                      htonl(static_cast<int32_t>(PacketType::SAFE))
```
```cpp
              // 다시 type 멤버에 집어넣기 위해 PacketType으로 캐스팅
              static_cast<PacketType>(htonl(static_cast<int32_t>(PacketType::SAFE)))
```
```cpp
// 이런 과정이 많음..
header.type = static_cast<PacketType>(htonl(static_cast<int32_t>(PacketType::SAFE)))
```
~~캐스팅의 마트료시카~~
그냥 캐스팅이 주구장창.. 캐스팅캐스팅캐스팅캐스팅캐스팅...

그래서 결국 제대로 프로토콜의 체계를 잡았어.

`PacketHeader` 구조체의 `type` 멤버처럼
**실제로 전송되는 값**은 그냥 raw value를 쓰기로 했고,
`PacketType`처럼 **연관된 상수를 하나로 묶어야하는 객체**는 그냥 `enum class`를 쓰기로 했어.

어쩔 수 없이 `PacketHeader` 구조체의 `type`멤버랑 `PacketType`의 멤버를 비교할 때는
`PacketType`의 멤버를 캐스팅해서 비교하는 식으로 하려고..

### 2-1. 배운 점

단순히 `enum class`와 raw value만이 아니라,

코드 안에서 의미를 표현하는 타입과,
연산되고, 변경되는 값은 서로 다른 계층으로 바라봐야한다는걸 알게된 것 같아.

내가 마주친 문제에서는
`PacketType`은 의미를 표현하기 위한 타입이고,
`PacketHeader::type`은 실제로 연산되고, 변경되는 raw value인거지.

`PacketType`과 `PacketHeader::type`은 서로 다른 계층의 값이야.
그래서 서로 다른 계층의 관점에서 바라보는거지.

그냥 어차피 같은 `int32_t`자료형으로 표현된다고 해도.

결국 코드를 잘 짜는게 단순히 효율적으로 짜는 것 뿐만 아니라,
이런 계층을 고려하며 짜는게 진짜 코드를 잘 짜는 것 같은 느낌이야.