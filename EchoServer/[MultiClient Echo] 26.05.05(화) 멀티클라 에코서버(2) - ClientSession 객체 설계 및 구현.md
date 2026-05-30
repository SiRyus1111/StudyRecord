## 0. 서론

이번에는 저번에 간단하게 설계했던 객체들 중에 
`ClientSession` 객체를 더 자세히 설계해보고, 구현해봤어.

이번 글에서는
- 기본적인 설계
  - 특히 각 `필드 / 메서드`의 `의미 / 역할`
- 실제 구현과 마주쳤던 문제점들
- 개발 로그

이렇게 세 파트로 설명할 것 같아.

일단 먼저,
내가 그린 객체 소유권 다이어그램을 첨부할게.
![](https://velog.velcdn.com/images/siryus0907/post/81ee7b4d-2a4c-4748-9882-41bc6ee8152d/image.png)
지난번에 설계했던 각 객체들이 어떻게 객체를 소유하고 있는제에 대한 다이어그램이야.
- 하늘색 - 소유
- 주황색 - 소유하지 않고 참조만
- 빨간색 - RAII(사실상 소유지만 일반적인 소유와 다르므로 분리)
- 연두색 - 스레드
- 초록색 - 단순 메모
- 검은색 - 이름(들)

이렇게 보면 될 것 같아.

---

## 1. 기본적인 ClientSession 객체의 설계

![](https://velog.velcdn.com/images/siryus0907/post/e67212f9-89b4-491b-af26-ad07b593d4eb/image.png)
![](https://velog.velcdn.com/images/siryus0907/post/98b7587f-90a9-4346-bd71-51b067e234c9/image.png)



이 객체는 클라이언트 하나당 하나씩 생성되는,
클라이언트 하나의 연결 단위로써의 클래스야.

그래서 각각의 클라이언트의 최대한 모든 정보를 담아.

(`private` 접근지정자)
- `unique_ptr<ClientSocket>` `ClientSock` 
  - 해당 클라이언트의 소켓을 객체로 감싸서 RAII를 적용한 `ClientSocket` 객체를 
  독점 소유하는 스마트 포인터
  - 오로지 해당 클라이언트와 통신하는 소켓을 담은 객체이므로 해당 ClientSession을 제외한
  다른 ClientSession이 소유하면 안됨.
  - 그래서 `unique_ptr`로 설정. `std::move`로 초기화
  - 해당 소켓을 사용하는 송 / 수신 함수는 해당 객체에 있음.
- `sockaddr_in` `ClientAddr` - 해당 클라이언트의 주소(IP:Port)를 저장하는 소켓 주소 구조체
  - 해당 구조체의 크기도 작아서 단순 값 복사를 해도 크게 상관없고 
  소유권을 나타낼 필요도 굳이 없어서 단순 값 복사로 초기화.
- `weak_ptr` `Manager_wp` - `ClientManager` 객체의 생명 주기에 영향을 주면 안되기 때문에,
`weak_ptr`로 소유하지 않고 참조만 함. + 순환 참조도 방지할겸..
- `NetState` `ClientState` - 송 / 수신 상태, 예외 상황을 기록할 상태 관리 구조체
  - 송 / 수신 시에 `ClientSockSend()` / `ClientSockRecv()`에 레퍼런스로 넘겨줌.
  - 그래서 `ClientSocket` 객체는 굳이 `ClientState`를 레퍼런스로 소유할 필요 없음.
  
![](https://velog.velcdn.com/images/siryus0907/post/2e129a70-53b0-49fc-bafb-077d64761934/image.png)
![](https://velog.velcdn.com/images/siryus0907/post/a717e2cd-2f50-417a-890a-c15f69c1b209/image.png)

(`public` 접근지정자)
- `ClientSession()` - 생성자
  - `ClientSock`은 `std::move`를 사용해서 초기화, `ClientAddr`은 값 복사로 초기화
  - `ClientState`는 모든 필드 전부 0(`false`)으로 명시적으로 초기화
    - `C++`에서는 구조체 생성할 때 초기화도 해주면 좋다고 해서 이렇게 함.
- `AddToManager(shared_ptr<ClientManager> sp)` 
  - `ClientManager`의 `shared_ptr`을 받아서 `Manager_wp`(`weak_ptr`) 초기화
- `Run()` - 송수신 함수
  - 원래는 송신 함수(`SendPacket()`)와 수신 함수(`RecvPacket()`)를 분리할 예정이었지만
  `ClientState`를 제대로 활용하기 어려워서(송 / 수신 예외 처리하기 어려움)
  송수신 과정 전체를 담당하는 `Run()`함수로 병합함. 
  - 나중에 구조가 안정된다면 `SendPacket()`과 `RecvPacket()`으로 분리할 예정
  + `SendHeaderErrorPacket()`도 할만할 듯.
  
이 정도가 되겠네.
설계하면서 그렸던 이미지에다가 실제로 구현한 것들을 참고해서 설명을 썼어.

---

## 2. ClientSession 객체의 구현

```cpp
class ClientSession {
private:
	std::unique_ptr<ClientSocket> ClientSock;
	sockaddr_in ClientAddr;
	std::weak_ptr<ClientManager> Manager_wp;
	NetState ClientState; // 단순 값 복사
public:	
	ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) : ClientSock(std::move(s)), ClientAddr(addr), ClientState{} { // move로 ClientSocket unique_ptr 객체 옮기기, addr 소켓 주소 구조체는 간단한 구조체이므로 단순 복사

	}

	// share_from_this()로 받기 / ClientManager 객체에서 사용하는 함수
	void AddToManager(std::shared_ptr<ClientManager> Manager_sp) {
		Manager_wp = Manager_sp;
		return;
	}

	void Run() {
		char buf[BUFFER_SIZE + 1];
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ClientAddr.sin_addr, addr, sizeof(addr));

		while (true) {

			PacketHeader recv_net_header{};
			
			ClientState.header_recv = true;
			int header_recv_res = ClientSock->ClientSockRecv(ClientState, (char*)&recv_net_header, sizeof(PacketHeader));

			if (header_recv_res == SOCKET_ERROR || header_recv_res == 0) {
				break;
			}
			ClientState.header_recv = false;

			PacketHeader recv_host_header{};
			recv_host_header.type = ntohl(recv_net_header.type);
			recv_host_header.length = ntohl(recv_net_header.length);

			if (recv_host_header.length > BUFFER_SIZE) {
				ClientState.if_header_error = true;
				break;
			}

			ClientState.payload_recv = true;
			int payload_recv_res = ClientSock->ClientSockRecv(ClientState, (char*)buf, recv_host_header.length);

			if (payload_recv_res == SOCKET_ERROR || payload_recv_res == 0) {
				break;
			}
			ClientState.payload_recv = false;

			buf[payload_recv_res] = '\0';

			std::cout << "송신한 클라이언트 : IP 주소 = " << addr << " 포트 번호 = " << ntohs(ClientAddr.sin_port) << '\n';
			std::cout << "받은 총 바이트 수 : " << payload_recv_res + header_recv_res << " 받은 메시지 : " << buf << '\n';

			if (recv_host_header.type == HEADER_ERROR) {
				ClientState.if_peer_error = true;
				break;
			}

			PacketHeader send_net_header{};
			send_net_header.length = htonl(payload_recv_res);
			send_net_header.type = htonl(SAFE);

			ClientState.header_send = true;
			int header_send_res = ClientSock->ClientSockSend(ClientState, (char*)&send_net_header, sizeof(PacketHeader)); // PacketHeader 구조체에는 패딩 없음.

			if (header_send_res == SOCKET_ERROR) {
				break;
			}
			ClientState.header_send = false;

			ClientState.payload_send = true;
			int payload_send_res = ClientSock->ClientSockSend(ClientState, buf, recv_host_header.length);

			if (payload_send_res == SOCKET_ERROR) {
				break;
			}
			ClientState.payload_send = false;

		}
		
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
			protocol_err_header.type = htonl(HEADER_ERROR);
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

	// ClientSession 객체 복사 방지용
	ClientSession& operator=(const ClientSession& c) = delete;
	ClientSession(const ClientSession&) = delete;

	~ClientSession() {
		if (std::shared_ptr<ClientManager> locked = Manager_wp.lock()) {
			// 여기서 ClientManager의 공유 컨테이너에서 해당 클라이언트 제거하는 함수 호출? 아직 결정난건 아님.
		}
	}
};
```

전체 구현은 이런데,
`Run()` 함수가 매우 길어.
이게 
- 헤더 수신
- 페이로드 수신
- 헤더 송신
- 페이로드 송신
- 예외 처리
- Protocol Error일 때 에러 메시지 송신

이런 것들이 전부 이 `Run()` 함수 안에 있어서..
그리고 이 로직들은 기존의 에코 서버의 코드를 사실상 복붙한 수준이야.

그저 단순히 `ClientSocket`에 접근하는 방식과
- 기존에는 바로 `client_sock.ClientSockSend()`
- 현재에는 이렇게 역참조 연산자로 `ClientSock->ClientSockSend()`

변수들의 이름
- `server_state` $\rightarrow$ `ClientState`
- `client_sock` $\rightarrow$ `ClientSock`
- 등등..

정도밖에 변한게 없어.

그래서 `Run()`함수에 대한 설명은 생략할게. 기존 에코 서버의 송 / 수신 로직에서 변한게 없음..

---

```cpp
class ClientSession {
private:
	std::unique_ptr<ClientSocket> ClientSock;
	sockaddr_in ClientAddr;
	std::weak_ptr<ClientManager> Manager_wp;
	NetState ClientState; // 단순 값 복사
public:	
	ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) : ClientSock(std::move(s)), ClientAddr(addr), ClientState{} { // move로 ClientSocket unique_ptr 객체 옮기기, addr 소켓 주소 구조체는 간단한 구조체이므로 단순 복사

	}

	// share_from_this()로 받기 / ClientManager 객체에서 사용하는 함수
	void AddToManager(std::shared_ptr<ClientManager> Manager_sp) {
		Manager_wp = Manager_sp;
		return;
	}

	void Run() {
		// 기존 에코 서버의 송 / 수신 로직
	}

	// ClientSession 객체 복사 방지용
	ClientSession& operator=(const ClientSession& c) = delete;
	ClientSession(const ClientSession&) = delete;

	~ClientSession() {
		if (std::shared_ptr<ClientManager> locked = Manager_wp.lock()) {
			// 여기서 ClientManager의 공유 컨테이너에서 해당 클라이언트 제거하는 함수 호출? 아직 결정난건 아님.
		}
	}
};
```

실제로는 이 정도로 볼 수 있어.

일단, `private` 접근 지정자에
- `std::unique_ptr<ClientSocket>` `ClientSock`
- `sockaddr_in` `ClientAddr`
- `std::weak_ptr<ClientManager>` `Manager_wp`
- `NetState` `ClientState`

이렇게 앞서말한 설계가 그대로 들어가있어.

그리고 생성자, `ClientSession()`인데,
여기는 중괄호 내부에는 아무것도 없고,
```cpp
ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) : 
ClientSock(std::move(s)), 
ClientAddr(addr), 
ClientState{} { // move로 ClientSocket unique_ptr 객체 옮기기, addr 소켓 주소 구조체는 간단한 구조체이므로 단순 복사

}
```
이렇게 멤버 초기화 리스트를 사용해서
`ClientSock`, `ClientAddr`, `ClientState`를 초기화했어.
이것도 설계와 똑같이,
- `ClientSock` - `std::move()`로 받은 `unique_ptr`이 소유한 객체의 소유권을 
`ClientSock`으로 이동시켜서 초기화
- `ClientAddr` - 단순히 값 복사로 초기화
- `ClientState` - 모든 필드 0으로 초기화

이렇게 초기화했어.

여기서 `unique_ptr`을 초기화할 때 문제가 발생했었는데,
밖에서는 
```cpp
std::shared_ptr<ClientSession> client_session = std::make_shared<ClientSession>(std::move(client_socket), client_addr);
```
이렇게 생성자에 인자들을 넣어서
기존의 `unique_ptr`인 `client_sock`이 소유하고 있던 
`ClientSocket` 객체의 소유권을 넘겼어서,
딱히 내부에서는 그냥 멤버 초기화 리스트에 `std::move()`를 사용할 필요가 없는 줄 알고
```cpp
ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) : 
ClientSock(s), 
ClientAddr(addr), 
ClientState{} { // move로 ClientSocket unique_ptr 객체 옮기기, addr 소켓 주소 구조체는 간단한 구조체이므로 단순 복사

}
```
이런 식으로 코드를 작성했었어.

그런데 결국 `std::move(client_socket)`으로 받아온 `ClientSocket` 객체의 소유권은 
`s`에 저장이 되고,
그 `s`에서 한번 더 소유권을 `ClientSock(std::move(s))`으로 옮겨줬어야 했었어.

그냥 단순히 
내 `std::move()`와 멤버 초기화 리스트에 대한 지식이 부족해서 생긴 문제였다고 볼 수 있겠네.

그리고 `AddToManager()` 함수는,
정말 간단하게
```cpp
void AddToManager(std::shared_ptr<ClientManager> Manager_sp) {
	Manager_wp = Manager_sp;
	return;
}
```
이렇게 `shared_ptr<ClientManager>`을 받아서 
`weak_ptr`인 `Manager_wp`를 초기화해주는 코드가 끝이야.

그리고 이 함수는 `ClientManager`쪽에서 
소유하고 있는 `ClientSession`을 역참조해서 호출해주는 용도의 함수야.

그 쪽에서 `AddClient()`를 호출하면 
`ClientSession` 객체에다가 해당 `ClientManager`의 `shared_ptr`을 줘서
`Manager_wp`를 초기화하는 로직이야.

그리고 물론 이 객체도 복사는 금지를 했어.
```cpp
// ClientSession 객체 복사 방지용
ClientSession& operator=(const ClientSession& c) = delete;
ClientSession(const ClientSession&) = delete;
```
이렇게.

클라이언트의 정보를 저장하는 객체인 `ClientSession`이 복사되면,
다양한 문제가 생길게 보였어.
- 일단 소유하고 있는 객체인 `ClientSocket`부터가 복사 불가능 객체
- 나중에 `Session ID`나 `NickName` 멤버도 추가되면 
중복 `Session ID` / `NickName`으로 인한 문제 발생

이런 것들.

물론 move는 가능함. copy가 안될 뿐이지.

## 3. 개발 로그

### 26.05.04

- 각 객체 간의 관계 설계
  - IdeaNote 참조
- `unique_ptr`, `shared_ptr` 관련 Syntax Error 해결
  - `std::move()`함수에 대한 이해 부족이 원인.
    - 생성자의 인자로 `unique_ptr<ClientSocket>`을 move를 이용해 받아왔지만, 객체 내부에 `unique_ptr<ClientSocket>`인 ClientSock에 소유권을 이전할 때 `std::move()` 함수를 사용하지 않았음.
	- 생성자 내부에서도 `std::move()` 함수로 ClientSock에 소유권을 이동시켜서 해결.
  - 추가적으로, `main()` 함수의 `shared_ptr<ClientSession>`을 생성하는 부분에서 오류 발생.
	- 초기화가 문제였음. 단순히 `shared_ptr<ClientSession> client(std::move(client_socket), client_addr)`로 잘못된 방식이었음.(생성자인줄 알고 생성자처럼 인자를 넣어버림)
	- `std::make_shared()` 함수로 초기화하여 해결.
- 서버 코드에 IdeaNote에서 설계한 `ClientSession`을 직접 코드로 구현
  - `ClientSession`
    - 클라이언트 하나의 연결 단위
	- 해당 클라이언트의 ClientSocket 소유(`unique_ptr`)
	- 해당 클라이언트의 주소, 송수신 상태 저장(단순 값)
	- 필요시 ClientManager 참조(소유하지 않음. `weak_ptr`)
	- 1 client_thread() <-> 1 ClientSession 구조
	- private 접근 지정자
	  - `unique_ptr<ClientSocket>` ClientSock 
		- raw SOCKET을 객체로 감싼 RAII 객체의 `unique_ptr`. 
		- `unique_ptr`을 사용해서 해당 클라이언트의 ClientSession 객체만 해당 클라이언트의 `ClientSocket` 객체를 소유할 수 있음을 명시.
		- send_all(), recv_all()은 이 객체가 함, 복사 불가, 단독 소유.
		- 이건 이미 지난번에 구현해놓음.
      - `sockaddr_in` ClientAddr 
		- 클라이언트 IP 주소를 저장할 소켓 주소 구조체. 
		- 이것도 이미 지난번에 구현해놓음.
      - `weak_ptr<ClientManager>` Manager_wp 
		- ClientManager 소유하지 않고 참조만 할 용도의 `weak_ptr`.
		- 이것도 이미 지난번에 구현해놓음.
      - `NetState` ClientState 
		- 송 / 수신의 현재 과정과 예외 상황을 기록할 상태 관리 구조체.
		- `ClientSocket`이 `ClientState&`를 멤버로 들고 있을 필요는 없음.
		- `ClientSession`이 `ClientState`를 값으로 소유하고, 송 / 수신 시 `ClientSockSend()` / `ClientSockRecv()`에 레퍼런스로 넘겨 상태 변화를 반영.
    - public 접근 지정자
	  - `ClientSession()` - 생성자
		- 받은 `unique_ptr<ClientSocket> s`를 `std::move(s)`로 멤버 `ClientSock`에 이동시켜 초기화.
		- `ClientAddr`은 값 복사로 받아서 초기화. - 간단한 값일 뿐이므로. (소유권 상관없음)
		- `ClientState`는 모든 필드 0(false)로 초기화. - 혹시 모를 쓰레기값이 있을까봐.
	  - `void AddToManager(shared_ptr<ClientManager> manager_sp)`
		- `shared_ptr<ClientManager>`를 받아서 `Manager_wp`를 초기화하는 함수
	  - `void Run()`
		- 이 함수는 아예 갈아엎음.
		- 원래는 송 / 수신 함수를 분리할 예정이었지만, `ClientState`를 보고 각 에러에 맞는 메시지를 출력, 에러 메시지를 클라이언트에 전송하는 과정을 넣기 어려울 것 같아서 `Run()` 함수로 송 / 수신 과정을 통합.
		- 이전의 에코 서버와 로직 변경 없음. 기존의 에코 서버의 송 / 수신 코드 사실상 복붙함.
		- 딱 변수 이름 + 포인터 역참조 변경밖에 변경점 없음.
		- 이거 하다가 토할뻔 함.
		- 나중에 구조가 안정된다면 `RecvPacket()`, `SendPacket()`, `SendHeaderErrorPacket()` 같은 함수로 분리할 수 있음.