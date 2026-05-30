## 0. 서론

이제 `shared_ptr`과 `weak_ptr`의 기본 개념을 배웠고,
다음으로 `unique_ptr`과 멀티스레딩 + 동기화 라이브러리를 공부하기 전에
기본적인 서버의 구조를 설계하려고 해.

아직 `unique_ptr`을 배우지는 않았지만,
결국 `unique_ptr`의 사용법만 모를 뿐
> 해당 객체의 소유권을 해당 `unique_ptr`만 가지고,
소유권의 복사(공유)는 허용하지 않고 이동만 허용한다.

라는 `unique_ptr`의 원칙은 알고 있기 때문에,
기초적인 설계는 할 수 있을 것 같았어.

이번에 설계한 객체는
- ClientManager
- ClientSession

정도가 있고,
앞으로 설계할 함수는
- client_thread - 말 그대로 각각의 클라이언트들을 담당할 스레드

이미 설계한 객체는
- WinsockGuard
  - 윈속의 초기화 / 종료를 생성자 / 소멸자에 묶는 객체
- ClientSocket 
  - 클라이언트와 통신할 소켓이라는 자원을 중심으로
    - 생성자로 소켓 받아와서 내부의 client_sock에 저장
    - ClientSockSend() - 상태 관리 구조체와 메시지 / 메시지의 길이를 받아서 길이만큼 송신
    - ClientSockRecv() - 상태 관리 구조체와 버퍼 / 버퍼의 길이를 받아서 길이만큼 수신
    - 복사(copy) 금지
    - 이동(move)만 가능
- ListenSocket
  - 클라이언트의 연결 요청을 받아서 처리할 
  Listen용 소켓이라는 자원을 중심으로
    - 기본 세팅 
      - socket() - 생성자로 listen용 소켓 생성
      - setsockopt() - SO_REUSEADDR
      - bind() - 바인딩할 주소가 저장되어있는 소켓 주소 구조체 필요
      - listen() 
    - 클라이언트의 연결 요청 수락(백로그 큐에서 꺼내와서 ClientSocket 생성)
      - ListenSockAccept() 
        - 클라이언트의 주소를 저장할 소켓 주소 구조체 필요
        - 반환된 ClientSocket은 copy 불가능 move만 가능
        - `ClientSocket`은 raw `SOCKET`을 소유하므로 복사는 금지하고 이동만 허용.
        - 현재 설계에서는 `ClientSession`이 `unique_ptr<ClientSocket>`으로 
        반환된 `ClientSocket`을 독점 소유.
    
이 정도가 있겠네..

## 1. 기본적인 객체 설계 - 소유권(ownership) 중심

그리고 각 객체의 자원 소유 관계는
```
ClientManager
  └── shared_ptr<ClientSession>

client_thread()
  └── shared_ptr<ClientSession>

ClientSession
  └── unique_ptr<ClientSocket>

ClientSocket
  └── SOCKET
```
이렇게 돼.

1. `ClientSocket`은 raw `SOCKET`을 객체로 감싼 RAII 객체
2. `ClientSession`은 해당 클라이언트의 세션의 모든 정보를 저장하는 객체
    - 복사 금지
    - `unique_ptr<ClientSocket>` `ClientSock`
      - unique_ptr로 
      해당 클라이언트의 ClientSocket 객체는 
      오로지 해당 ClientSession 객체만 소유할 수 있음을 명시.
    - `sockaddr_in` `ClientAddr`
    - `weak_ptr<ClientManager>` `Manager_wp`
      - 서버를 소유하지는 않고 참조만 하기 위한 `weak_ptr`
      - 이건 나중에 `AddToManager` 함수를 `ClientManager`에서 역참조로 호출해서
      `ClientManager` 객체의 `shared_ptr`을 넘겨받아서 초기화
    - `AddToManager()` 함수
      - `shared_ptr<ClientManager>`를 받아서
      내부의 `weak_ptr<ClientManager>`가 같은 `control block`을 관찰하도록 설정함.
      - 즉, `ClientSession`이 `ClientManager`를 소유하지는 않지만,
      필요할 때 `lock()`을 통해 아직 살아있는지 확인할 수 있도록..
3. `ClientManager`는 모든 `ClientSession`을 관리하는 Manager 클래스
    - 복사 금지
    - 공유 컨테이너(현재는 임시로 vector 사용)로 클라이언트들의 정보를 저장
      - 나중에 채팅 서버로 확장할 때를 대비해서 미리 설계
        - 브로드캐스트, 특정 사용자 킥(kick)같은 기능 추가할 때 필요
      - `vector<shared_ptr<ClientSession>>` `clients`
        - ClientSession 객체를 복사하지는 않고 `shared_ptr`로 소유하는 식으로 관리
    - `mutex` `client_mutex`
      - clients에 접근할 때의 동기화를 담당할 뮤텍스
      - 동기화 라이브러리를 제대로 배우고 사용할 예정
    - `AddClient` 함수
      - `ClientSession` 객체의 `shared_ptr`(`shared_ptr<ClientSession>`)을 받기
      - `client_mutex`로 `lock`
      - `clients` 컨테이너에 삽입(현재는 `vector`이므로 `push_back()`)
      - 해당 객체의 `AddToManager()` 함수를 호출,
      인자는 `control block`을 공유하도록 `shared_from_this()` 함수로 넘기기
      해당 객체의 `Manager_wp`를 초기화
4. client_thread() 함수는 말 그대로 각각의 클라이언트들을 담당할 스레드
    - 여기서 송 / 수신 루프 진행 예정
    - accept() 까지 완료 후 ClientSession 객체를 shared_ptr로 넘겨주기
    ClientManager 객체도 해당 ClientSession을 소유해야하므로 shared_ptr을 사용
    - NetState 구조체는 이 함수 내에서 생성 후 ClientSession 객체에 집어넣을 예정.
5. 기타 사항
   - `ClientSession` 객체는 특정 스레드 하나만 독점 소유하는 객체가 아니라,
`ClientManager`와 `client_thread()`가 함께 생명주기를 보장해야 하는 객체이므로
`shared_ptr<ClientSession>`으로 관리.
   - `ClientSession`은 `shared_ptr`로 여러 곳에서 참조될 수 있지만, 그 내부의 `ClientSocket`까지 공유되어야 하는 것은 아님.
   - 한 클라이언트와 연결된 실제 소켓 자원은 
   해당 `ClientSession` 하나가 책임지는 것이 자연스럽기 때문에, 
   `ClientSocket`은 `ClientSession`이 `unique_ptr<ClientSocket>`으로 독점 소유하도록 설계함.
   
### 1-1. 정리
   
정리하면, 이 정도.

- `SOCKET`은 `ClientSocket`이 RAII로 관리한다.
- `ClientSocket`은 `ClientSession`이 `unique_ptr`로 독점 소유한다.
- `ClientSession`은 `ClientManager`와 `client_thread()`가 함께 사용해야 하므로 `shared_ptr`로 관리한다.
- `ClientSession`은 `ClientManager`를 소유하지 않고 참조만 해야 하므로 `weak_ptr`를 사용한다.
- `ClientManager`는 여러 `ClientSession`을 공유 컨테이너에 저장하고, mutex로 접근을 보호한다.

### 05.04에 그린 다이어그램

![](https://velog.velcdn.com/images/siryus0907/post/2d3a6143-f834-438d-bf9e-b83e9276c2c0/image.png)
   
## 2. 실제 코드 짜면서 적은 개발 로그

### 26.05.02

- 서버 코드에 새로 배운 shared_ptr / weak_ptr / unique_ptr(의 뼈대) 추가
  - 자원과 객체의 소유권 관점
  - unique_ptr은 아직 배우지 않음
    - 배운 후에 제대로 갈아엎을 예정.
  - ClientManager 클래스
	- 클라이언트들을 관리할 클래스
	- 채팅 서버의 브로드 캐스트 등의 기능을 고려해서 미리 설계
	- 일단 명시적으로 복사 방지 코드 넣어놓음. 공유 컨테이너 때문에 복사하면 안됨..
	- public std::enable_shared_from_this
	- std::vector<std::shared_ptr<ClientSession>> clients - 클라이언트들의 정보(shared_ptr) 들을 소유할 ClientManager 클래스의 컨테이너. 
	  - 스택 영역의 객체인지 힙 영역의 객체인지 추가적인 고려 필요.
	  - 나중에 추가로 다른 컨테이너도 고려해볼 예정
	- std::mutex client_mutex - clients 공유 컨테이너에 접근할 때 사용하는 lock.
    - void AddClient(std::shared_ptr<ClientSession> client) - clients에 client_mutex를 사용한 lock과 함께 클라이언트의 shared_ptr을 push_back()
	  - std::lock_guard는 지금 상태에선 로그에는 못 쓰겠다.. 주석 참고
      - shared_from_this()로 control block을 고려해서 ClientManager 객체의 shared_ptr을 ClientSession 객체에 전달
  - ClientSession 클래스
	- 클라이언트들의 각 세션을 표현할, 각 세션의 정보를 담고있는 클래스
	- 일단 명시적으로 복사 방지 코드 넣어놓음. 어차피 이 객체는 client_thread 함수에 move semanstic으로 넘겨줄 생각.. 이었는데
	  - 그냥 ClientManager 클래스도 ClientSession을 소유할 필요가 있으므로 shared_ptr로 넘길 듯..
	- std::unique_ptr<ClientSocket> sock - raw socket의 정보와 raw socket으로 가능한 행동(send / recv 등)이 들어있는 ClientSocket 객체. 
	  - unique_ptr로 해당 ClientSocket 객체를 독점적으로 소유한다고 명시.
	- sockaddr_in addr - ClientSocket(sock) 객체의 SOCKET의 주소가 저장되어있는 구조체. 
	  - 이건 나중에 ClientSocket 객체 내부로 옮기는 것도 고려해봐야 할듯.
	- std::weak_ptr<ClientManager> Manager_wp - ClientManager 객체의 shared_ptr로부터 받을 weak_ptr. 
	  - cyclic reference를 막고 ClientManager 객체의 lifetime에 간섭하면 안되는 하위 객체이기 때문에 weak_ptr을 씀.
	  - 객체 생성 시점에서는 ClientManager 객체의 shared_ptr을 받는 구조가 아니므로, shared_ptr을 복사하는 것으로 초기화는 하지 않음.
    - ClientSession(std::unique_ptr<ClientSocket> s, sockaddr_in addr) -  ClientSession 객체의 생성자. 
	  - unique_ptr로, ClientSocket 객체를 이동(move)이라는 semantic으로, sock으로 받아오기.
      - 물론 해당 ClientSocket 객체의 소켓 주소 구조체도 받아서 ClientSession 객체의 addr으로 받아오기.
		- 이건 단순 복사로 처리해도 상관없음. 지금은. 그래도 복사할 때의 오버헤드가 있긴 하고 재할당을 해야하니(레퍼런스 불가) unique_ptr로 받아오는 것으로 개선해도 뭔가 좋을 것 같음.
		- 아님. sockaddr_in은 소켓 주소 정보를 담는 값 데이터에 가깝기 때문에, 일단은 단순 복사로 처리해도 충분해 보임.
        - 나중에 IPv6까지 고려한다면 sockaddr_storage로 확장하는 것도 고려할 수 있을 것 같음.
    - void AddToManager(std::shared_ptr<ClientManager> Manager_sp) - ClientManager가 사용하는 ClientManager 객체의 shared_ptr을 넘겨주는 함수.
	  - shared_ptr(Manager_sp)를 받아서 weak_ptr에 복사해서 weak_ptr(Manager_wp) 초기화? 
		- 이거 초기화 맞나? 다음에 알아보자. 지금은 피곤해..
    - 소멸자에선 manager_wp.lock()으로 ClientManager 객체가 살아있는지 확인 후 반환된 shared_ptr로 ClientManager의 공유 컨테이너에서 해당 클라이언트 제거하는 함수 호출
	  - ..인데 이건 나중에 다시 봐야할 듯. 재검토 필요. 
- 추가적인 수정은 unique_ptr 공부한 후에 할 예정.
- 시발 내가 지금 뭘 한거지 시발 ㅈ됐다 못 쉬고 이런거 적어버렸다 시발 번아웃 상태인데 시발 진짜 ㅈ됐다