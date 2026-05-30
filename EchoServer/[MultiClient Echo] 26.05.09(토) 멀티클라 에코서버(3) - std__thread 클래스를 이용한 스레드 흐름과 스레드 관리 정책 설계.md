## 0. 서론

아직 `std::thread`관련 글을 다 쓰지는 않았지만,
너무 실제 설계에 `join()`과 `detach()`를 적용해보고 싶어서
한번 설계를 해봤어.

이번에 설계해본것들은

1. 멀티스레드에서의 각 스레드 흐름
2. `detach()`로 분리한 스레드의 상태 관리

이 두 가지 정도야.

## 1. `join()`을 써보려 했으나..

![](https://velog.velcdn.com/images/siryus0907/post/a82da610-b909-41ae-aec7-4ed28ff51982/image.png)

처음에는 `join()`기반으로 스레드를 관리하려했어.
하지만..

- `client_thread`의 종료 시점
- `ClientSession`의 제거 시점
- `ClientManager`의 관리 책임

이런 것들이 서로 얽히면서 구조가 지나치게 복잡해져서
이 구조는 포기했어.

막 
> 코드가 `join()`함수 다음으로 넘어가는걸 트리거로 
`ClientManager` 객체의 `clients`에서 해당 세션을 빼버리자!

> `client_thread` 종료 직전에 `ClientSession` 객체의 `Manager_wp`로 `ClientManager`의 `RemoveClient()` 함수를 호출해버리고 
`ClientManager` 객체에서 `join()` 함수를 호출해서 스레드가 종료된 것을 보장받은 후
`clients`에서 해당 세션을 빼버리자!

(애초에 첫 번째 아이디어가 두 번째 아이디어에 포함되지 않나? 이 때는 잘 몰랐음)
이런 아이디어가 있었는데,

그 당시에는 이런 아이디어를 직접 구조화하거나 구현하는게 너무 복잡할 것 같아서
이런 아이디어는 지금 당장은 포기한 상태야.

그래서 `detach()` 기반의 `client_thread` 관리로 기본 뼈대를 잡았어.

## 2. `detach()` 기반의 각 스레드의 흐름

일단 main 스레드와 `detach()`의 흐름을 직접 시각화해봤어.
![](https://velog.velcdn.com/images/siryus0907/post/eca08be0-3632-44d5-81ea-e8f72f5fbdc2/image.png)
이렇게. (너무 하단 여백이 커서 추후에 이미지 절삭 예정)

중간에 빠진게 조금 있어.
별 중요한건 아니긴 한데,
여기서는 빠진 것도 전부 다 쓸게.

`메인 스레드`는 

1. 기본 세팅 - `WinsockGuard` / `ListenSocket` 객체 생성 및 `ListenSocket`에 서버의 IP:Port 바인딩
2. 각 클라이언트를 `accept()`하는 `while`루프 진입 - `반복 조건 == true`(무한 반복)
2.1. `while`루프 진입 후 `accept()` 준비(클라이언트 소켓 주소 구조체 생성)
2.2. `ListenSocket` 객체의 `ListenSockAccept()` 함수로 
`accept()` 진행 후 `ClientSocket` 객체 반환
2.3. `shared_ptr`의 `ClientSession` 객체 생성 - 생성자에 `ClientSocket` 객체 `std::move()`로 집어넣고
클라이언트 소켓 주소 구조체도 집어넣기
2.4. `ClientSession`을 `ClientManager`의 `clients`에 `AddClient()` 함수로 집어넣기
2.5. `client_thread`를 첫번째 인자(실행할 스레드(함수))로 하는 `std::thread` 객체 생성
2.6. `client_thread`(실제 실행 흐름)를 `detach()`로 `std::thread` 객체에서 완전히 분리하기
3. `closesocket()` / `WSACleanup()` 등은 `RAII`를 적용한 객체들로 인해서 자동으로 호출됨

`client_thread`는

1. `ClientSession`객체의 `Run()`함수 실행
1.2. `Run()` 함수 내에서 송 / 수신 진행 (기존 로직)
1.3. 만약 스레드를 종료해야하는 예외 상황(`transport error` / `peer exit` / `protocol error`)
발생시 별도로 `ClientSession`에 명시하기(그림에서는 `closing` 변수를 `true`로 변환)
1.4. `closing` 변수를 `true`로 바꾼 후 `ClientManager`객체의 `RemoveClient()`를 호출해서
해당 `client_thread`가 `shared_ptr`로 공유 소유하고있는 `ClientSession` 객체를 
`ClientManager`의 `clients`에서 `remove`하기
1.5. 그 후 `Run()` 함수 종료
2. 스레드 종료

이렇게 전체적인 흐름을 시각화해봤어.

## 3. `detach()`로 분리한 스레드의 상태를 어떻게 알 수 있을까?

이건 그저 
> `ClientManager`가 `client_thread`가 종료될 예정이란걸 안다면
추후 채팅 서버에서 브로드캐스트를 진행할 때
해당 `client_thread`가 `shared_ptr`로 `ClientManager`와 공유 소유하고있는 
`ClientSession`에 접근하지 않게 함으로써 좀 더 최적화를 할 수 있지 않을까? 
종료될 예정인 `client_thread`에 `send()`하는걸 어느정도 예방할 수 있지 않을까?

> `join()`을 썼다면 `client_thread`의 종료를 확실히 `std::thread` 객체만 보고도 알 수 있지만
`detach()`를 써서 `std::thread` 객체가 실제 스레드(`client_thread`)와 연결되어있지 않기 때문에
`ClientManager`가 해당 스레드가 종료되었는지 바로 알 수 없잖아.

> 그러니까 따로 스레드가 무조건 종료할 예정일 때
(`transport error` / `peer exit` / `protocol error`)
`client_thread`에서 `ClientSession`객체의 `closing`에 따로 명시해줘서
`ClientManager`가 그걸 보고 해당 `client_thread`가 종료될 예정이란걸
알 수 있게 하자.

라는 발상에서 시작됐어.

근데 이게 결국 물리적 종료 상태와 논리적 종료 상태를 구분하는거더라고..

- `shared_ptr<ClientSession>` = 물리적 생존 보장
- `closing` 변수 = 논리적 종료 상태 표시
- `ClientManager::RemoveClient()` = 그저 `ClientManager`의 관리 대상 `ClientSession`에서 
해당 `ClientSession` 객체 제외

이렇게.

그래서 결국, 이 아이디어는
> `detach()` 기반 구조에서는
`std::thread` 객체로 분리된 스레드의 상태를 직접 관측할 수 없다.

> 그러므로 해당 스레드의 상태를 관측할 수 있는
다른 방법이 필요하다.

이게 핵심이었어.

그래서, 현재 구조에서는
`ClientSession` 객체 내부에 `closing`이라는 `std::atomic<bool>` 변수를 추가해서
해당 스레드가 관리하는 `ClientSession`이 종료 예정 상태인지 기록할 수 있게 했어.

![](https://velog.velcdn.com/images/siryus0907/post/3fbfac2b-9216-4205-b68d-50b59fe7dc2a/image.png)

코드로는,
```cpp
class ClientSession {
private:
    std::atomic<bool> closing = false;
};
```
이렇게.

굳이 `atomic` 변수로 선언한 이유는
- 이 변수에 접근할 때 데이터 레이스가 발생하면 안됨.
- 딱 변수 하나만 동기화를 해주면 되고 상대적으로 자주 수행될 수 있는 연산임.

그래서 변수 하나 동기화를 하는데는 상대적으로 오버헤드가 적은 `std::atomic`을 동기화 방식으로 사용했어.

그래서 전용 함수로 해당 변수의 값을 바꾸지.
(`store()`)


- `closing == false`라면 아직 정상 송수신 가능한 세션
- `closing == true`라면 `ClientSession` 객체는 아직 살아있지만,
더 이상 정상적인 송신 대상으로 취급하지 않는 종료 예정 세션

### 3-1. `Run()` 함수의 종료 흐름

`Run()` 실행 도중
`transport error`, `peer exit`, `protocol error` 등, 
즉 더이상 해당 세션을 유지할 수 없을 예외 상황이 발생하면
해당 세션을 종료 예정 상태로 변경해.

그 후 `RemoveThisClient()`를 호출해서
`ClientManager`의 `RemoveClient()`, 
즉 `ClientManager` 객체의 `ClientSession` 관리 목록(`clients`)에서 제거하는 요청을 보내지.

```cpp
closing.store(true);
RemoveThisClient();
// 예외 처리 / 판단 코드
return;
```

이런 식으로.

여기서 중요한건,
> `RemoveClient()`가 즉시 객체 소멸을 의미하지는 않는다

라는 점이야.

그저 관리 목록에서 제거. 
해당 객체는 아직 `client_thread`에서 참조되어있을 수도 있으므로
물리적으로는 살아있을 수 있다는거지.

물론 논리적으로는 `closing == true`에 의해 죽을 예정인 상태고.