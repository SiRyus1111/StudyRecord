## 0. 서론

멀티클라 에코서버 설계 전에 C++ 공부하는 것도 이제 절반?이네ㅋㅋ
사실 남은 주제들은 어렵고 복잡해서 아직 절반은 커녕 1/3이나 했는지 모르겠네..

1. 객체 표현과 객체의 생명 주기(완료)
2. C++의 RAII(완료)
3. 스마트 포인터(이번 글)
4. 스레드 관련 라이브러리 기본
5. 동기화 관련 라이브러리 기본

이번 주제는 세 번째 주제인 C++의 스마트 포인터,
즉 
- std::shared_ptr
- std::weak_ptr
- std::unique_ptr

이야.
공부할 책으로는 Nicolai M. Josuttis의
**C++ Standard Library - A Tutorial and Reference**라는 책을 사용했어.

이 책은 왜 C++의 표준 라이브러리들이 왜 이렇게 설계되었으며,
내부가 어떻게 설계되어있는지가 잘 나와있어서,
계속 왜? 왜? 왜?를 묻고 
단순 암기가 아닌 맥락적 이해식 암기를 주로 하는 나한테는
정말 최적의 책이라 선택했어.

~~물론 정말 C++ 표준 라이브러리의 바이블로 불리는 책이라 선택한 것도 있고~~

일단 이번 글에서는,

- 5.2.1 - Class `shared_ptr`
- 5.2.2 - Class `weak_ptr`
- 5.2.5 - Class `unique_ptr`

이야. shared_ptr(스코프 벗어나도 조건부로 객체 소멸 안함)과 weak_ptr은 어느정도 연결점이 있고,
unique_ptr까지는 내 서버 프로젝트에 사용이 될 것 같아서(자원 복사 금지 / 자원 이동 가능)
이 정도 범위를 잡았어.

그리고 너무 깊게 들어가지 않기 위해서,
5.2.3 ~ 5.2.4절은 지금은 하지 않기로 했어.

일단 이번 글의 목차는,

1. 스마트 포인터란 무엇이며, 왜 필요한가
2. Class `shared_ptr`
3. Class `weak_ptr`
4. 객체의 소유권이란
5. Class `unique_ptr`

이렇게 5가지 챕터로 C++ 표준 라이브러리의 스마트 포인터에 대해 설명해볼게.

## 1. 스마트 포인터란 무엇이며, 왜 필요한가

우리가 C++에서 포인터를 사용한다는건,
결국 스코프 밖에서도 참조를 할 수 있게 하기 위해서라고도 할 수 있어.

예를 들어, 다른 함수는 분명 스코프 밖이지만,
포인터로 인자를 넘긴다면
그 주소에 있는 값을 참조하고, 
수정할 수 있게 되지.

하지만, 
- 포인터 변수 자체의 수명
- 포인터가 가리키는 주소(객체)의 수명

이 두가지를 일치시켜야 댕글링 포인터 / 자원 누수 등의 문제들이 발생하지 않아.

만약 포인터가 가리키는 객체의 수명이 끝났는데
포인터 변수가 살아있어서 해당 메모리 주소가 살아있다면
없는 주소를 참조하는 댕글링 포인터 문제가 생길 수 있어.

그리고 또 만약 객체에 대한 마지막 포인터까지 포인터에 의한 참조가 끝났는데
포인터가 가리켰던 객체가 소멸되지 않고 남아있게 된다면
자원 누수(메모리 누수) 문제가 생길 수 있어.

이런 일을 방지하고자, 스마트 포인터라는게 있어.

저번에 RAII에 대해 공부했었는데,
RAII란
> 객체의 생명 주기에 자원의 획득과 해제를 묶는 것

이라고 했었지.

스마트 포인터는 RAII의 대표적인 구현 예시야.

포인터가 가리키는 객체의 수명이 끝났다면
포인터 변수도 소멸시켜서 소멸된 객체를 참조할 수 없게 만들어 줌으로써
댕글링 포인터 문제를 막아줘.

객체에 대한 마지막 포인터까지 참조가 끝났다면
해당 객체의 마지막 소유자로써 해당 객체를 삭제함으로써
자원 누수 문제를 막아줘.

즉, 스마트 포인터는 포인터 변수의 수명과
포인터가 가리키는 객체의 수명을 연결시켜줘서

객체가 먼저 소멸되는 상황(댕글링 포인터)이나
객체가 끝까지 소멸되지 않는 상황(메모리 누수)을
방지해주는 도구야.

이렇게
> 포인터가 가리키는 객체의 소멸과 포인터 변수의 소멸을 묶어서 동작하는 포인터

를 스마트 포인터라고 해.

좀 더 상세히 설명해보자면,
스마트 포인터란,
> 객체의 소유권을, 그러니까 생명 주기를 책임지는 포인터

> 어떤 포인터가 객체의 소유권(ownership)을 가질 것인가?

> 어떤 포인터가 해당 객체를 delete할 책임이 있는가?

> delete 되었을 때 댕글링 포인터 문제를 해결하기 위해
해당 객체가 소멸되었을 시 해당 객체를 가리키는 포인터로 객체에 접근하는 것을 제한

그리고 하나의 스마트 포인터 클래스로는 다양한 사용처에 대한 부분을
충족시키기 어려워서,
C++에는 여러개의 스마트 포인터 클래스가 있어.

Josuttis 책이 설명하는 클래스, C++11 기준의 대표적인 스마트 포인터는 이렇게 두 가지야.
- `shared_ptr`
  - 소유권(ownership)을 공유(share)
  - 여러 스마트 포인터가 하나의 객체에 대한 소유권을 공유 가능
  - 마지막 참조가 끝난다면 객체가 소멸
  - `weak_ptr`, `bad_weak_ptr`, `enable_shared_from_this` 등의 헬퍼 클래스 제공
- `unique_ptr`
  - 소유권(ownership)을 공유하지 않음. 단독으로(exclusive) 소유
  - 한 스마트 포인터만 하나의 객체에 대한 소유권을 가질 수 있음
  - 하지만 소유권 이동(transfer)은 가능
  - `delete`를 호출하지 않은 경우나 `new`로 객체를 생성한 후 예외가 발생한 경우 등에서 
  발생할 수 있는 자원 누수를 예방 가능
  
대표적인 스마트 포인터 클래스는 이렇게 두 가지가 있고,
이번 글에서는 `shared_ptr`에 딸려있는 `weak_ptr`까지 추가로 설명해볼게.

이 주제의 내용을 정말 짧게 요약하면,
- 나만 저 객체를 단독으로 나만 갖고 싶으면 `unique_ptr`
- 저 객체를 여러명이서 같이 가질 필요가 있는데 
혹시 아무한테도 필요없게 됐을 때
안전하게 죽여버리고 싶으면 `shared_ptr`
- 그 여러명이서 가지고 있는 객체를 가지고 싶지는 않은데 
보고만 싶으면 `weak_ptr`.

그리고 모든 스마트 포인터 클래스는
`<memory>`헤더에 있어.
  
## 2. Class `shared_ptr`

우리는 복잡한 프로그램을 작성할 때,
웬만해서는 한 객체를 여러 스레드에서, 함수에서 참조하게 돼.

비록 기존의 포인터와 레퍼런스가 있었지만,
확실히 부족했어.

해당 객체에 대한 마지막 참조까지 끝나게 된다면, 
더이상 소유권(ownership)을 가진 포인터가 하나도 남지 않게 된다면
객체는 어느 곳에서도 사용하지 않는게 되니까 소멸되어야 해.

어느 곳에서도 사용하고 있지 않는다면, 참조하고 있지 않는다면 
해당 객체가 계속 남아있는건
메모리 누수 / 자원 누수니까.

그리고 그렇게 자동으로 해당 객체의 소멸자를 호출하고,
해당 객체가 소유하고 있는 메모리 / 자원을 해제하는 동작이 필요하겠지.

`shared_ptr` 클래스는 이런 기능을
**공유 소유권(shared ownership)**이라는 동작 규칙(semantics)으로 제공해.

semantics란, 쉽게 설명하면 
Syntax(문법)가 `코드 형태의 규칙`이라고 한다면,
Semantics(동작 규칙, 의미론)는 `실제 작성된 코드가 런타임에 내부적으로 어떻게 실행되는지에 대한 규칙`
이라고 보면 돼.

`a = b;`라는 코드가 있을 때:
- Syntax: "식별자 a, 대입 연산자 =, 식별자 b, 세미콜론 ;으로 구성됨" 
(형태 검사)
- Semantics: "b의 값을 a에 복사한다. 만약 b가 rvalue이라면 a로 이동(move)시킬 수도 있다." 
(실제 코드의 동작 정의)


여러개의 `shared_ptr`은 하나의 객체에 대한 
소유권을 공유(sharing ownership)할 수 있어.

그리고 마지막까지 해당 객체를 소유했던 위치에서는
해당 객체의 소멸자를 호출하고
해당 객체와 관련된 메모리 / 자원들을 정리하지.

요약하면,
> shared_ptr는 하나의 객체에 대한 소유권을 여러 shared_ptr들이 공유할 수 있게 해주고
객체가 더이상 필요하지 않을 때, 
그러니까 참조되어있지, 소유되어있지 않을 때
자동으로 해당 객체와 연관된 자원을 정리하는 스마트 포인터

라고 할 수 있어.

## 2-1. `shared_ptr`의 사용법과 동작

shared_ptr은 다른 포인터들 처럼 
- 할당하고
- 복사하고
- 비교하고

이렇게 사용할 수 있어.

그리고 다른 포인터들 처럼 역참조 연산자인 `*`나 `->`으로
포인터가 가리키는 주소를 참조할 수 있지.

```cpp
std::shared_ptr<std::string> pString1(new string("AaAaAaA"));
std::shared_ptr<std::string> pString2(new string("스마트포인터"));

(*pString1)[0] = 'B'; // pString1이 가리키는 문자열의 첫 글자를 'B'로 변경
pString2->replace(5, 1, "트"); // pString2가 가리키는 문자열의 6번째 글자를 '트'로 변경

*pString1 = "BbBbBbB"; // pString1을 BbBbBbB로 덮어쓰기
```

등등..

그리고 여기서 중요한게,
```cpp
std::shared_ptr<std::string> p1(new std::string("Hello"));
std::shared_ptr<std::string> p2 = p1;
```
이렇게 shared_ptr을 복사한다고 해서
shared_ptr이 가리키는 객체 자체가 복사되는 것은 아니고,
그저 해당 객체를 함께 소유하게 되는거야.

여기서는 p1과 p2가 같은 문자열 객체를 소유하게 돼.

그리고, 당연히
```cpp
*p2 = "World";
```
이렇게 함께 객체를 소유하고 있는 `shared_ptr`에
역참조로 객체의 상태를 변경한다면,
```cpp
std::cout << *p1 << '\n'; // World가 출력됨
```
이렇게 해당 객체를 소유하고 있는 다른 `shared_ptr`에 접근했을 때도
해당 객체가 바뀐 결과를 보게 되는거야.

여기까지는 그냥 기존의 포인터에 대한 설명과 같아.

> 객체의 주소값을 저장하고 역참조 연산자로 해당 객체에 접근한다.
그리고 해당 객체의 값이 바뀐다면
해당 주소를 참조하고 있는, 해당 객체를 소유하고 있는
다른 포인터들로 해당 객체에 접근했을 때도
해당 객체가 바뀐 결과를 보게 된다.

이 정도.

### 2-1-1. `shared_ptr`의 초기화

하지만 여기서부터가 핵심인데,
클래스 `shared_ptr`의 생성자는 `explicit`, 즉 명시적 생성자 키워드가 붙어있기 때문에,
```cpp
std::shared_ptr<std::string> pString = new std::string("스마트포인터"); // 에러 발생
```
이렇게 단순히 `new std::string("...")`을 대입하는 표현으로는
초기화할 수 없고,
```cpp
std::shared_ptr<std::string> pString(new std::string("스마트포인터"));
```
이렇게만 초기화할 수 있어.
그래서 기존의 포인터와 다르게 단순히 대입으로 초기화하려 하면 에러가 발생해.

왜 굳이 생성자에 `explicit`을 붙여서 대입 연산자로 초기화하지 못하도록 했는지 궁금할 수도 있는데,
결국 `new`로 생성되는 '객체'가 `shared_ptr`이 아닌 그저 원시 포인터(raw pointer)라서 그래.
```cpp
std::shared_ptr<std::string> pString = new std::string("스마트포인터");
```
이 코드도 그냥 일반적인 포인터 대입 연산처럼 보이지만,
실제로는 일반 포인터의 소유권을 `shared_ptr`객체에게 넘기는 작업이야.

그래서, 이런 소유권 이전은 명시적으로 드러나는 편이 안전하기 때문에,
암시적 형 변환을 막고 
```cpp
std::shared_ptr<std::string> pString(new std::string("스마트포인터"));
```
이렇게 명시적으로 `shared_ptr`을 생성하도록 하는거야.
단순 안전의 이유임.

이게 뭐가 명시적이냐고 할 수 있는데, 
단순 대입 연산자는 raw pointer -> `shared_ptr`으로의 암시적 형 변환이 일어날 수 있지만,
이렇게 생성자에 직접 넘기면 생성자 자체에서 raw pointer를 받아서
그걸 `shared_ptr`로 명시적으로 변환해줘.

그리고 더 편리하게 초기화할 수 있게 해주는 `make_shared()` 함수도 있는데,
```cpp
std::shared_ptr<std::string> pString = std::make_shared<std::string>("스마트포인터");
```
이렇게 사용할 수 있어.
이렇게 `shared_ptr` 객체를 초기화하면 확실히 더 빠르고 안전하게 초기화할 수 있어.

왜 그렇냐면, 일반적인 
```cpp
std::shared_ptr<int> p(new int())
```
이런 방식은 객체 자체의 메모리 할당과 
해당 객체를 참조하는 `shared_ptr`들의 공유 정보를 저장하는 제어 블록(`control block`)이라는 것을
각각 한 번씩, 총 두 번에 걸쳐서 할당하는데,

`make_shared()` 함수를 사용하면 단 한 번의 메모리 할당만으로도
객체 자체의 메모리 할당과, 제어 블록의 메모리 할당을 동시에 해버릴 수 있어.

그리고 `make_shared()` 함수는 객체를 **생성**하는 함수인지라,
이미 생성되어있는 객체에 `shared_ptr`로 참조하려면
```cpp
T* ptr = new T(); // 이미 생성되어있는 객체(동적 할당, 힙 영역에 생성)
std::shared_ptr<T> p(ptr);
```
이런 식으로 `shared_ptr`을 생성해야해.

물론 다른 방법도 있는데,
```cpp
std::shared_ptr<std::string> pString;

pString.reset(new std::string("스마트포인터"));
```
이렇게 `shared_ptr` 객체를 생성만 하고 
`reset()` 함수를 사용해서 유사 대입해주는 식으로..

그리고 `reset()` 함수에 아무런 인자를 넣지 않는다면,
사실상 해당 `shared_ptr`은 해당 객체의 소유권을 포기하는 것과 같아.
이거 나중에 엄청 유용함.

### 2-1-2. use_count()와 reference count

그리고, `use_count()` 함수로 해당 `shared_ptr`가 가리키고 있는 객체를 
소유하고 있는
`shared_ptr`의 갯수를 알 수 있어.

```cpp
auto count = pString.use_count();
```
이렇게.

이게 뭐가 중요한가 싶지만,

특정 객체에 대해 더이상 소유권(ownership)을 가진 포인터가 하나도 남지 않게 된다면
해당 객체는 어느 곳에서도 사용하지 않는게 되니까 소멸되어야 한다고 했지?

그리고 자동으로 해당 객체의 소멸자를 호출하고,
해당 객체가 소유하고 있는 메모리 / 자원을 해제하는 동작이 필요하다고도 했고.

그렇다면, 그렇게 해당 `shared_ptr`이 
소유하고 있는 객체의
마지막 소유자인걸 어떻게 알 수 있을까?

그게 `control block`(제어 블록)의 `(strong) reference count`라고 하는, 
해당 `shared_ptr`이 소유하고 있는 객체에서
총 몇 개의 `shared_ptr`이 소유권을 공유하고 있는지 나타내는
`shared_ptr`의 숨겨진? 포인터야.

그래서 `shared_ptr`은 내부적으로 이렇게 두 개의 정보를 관리한다고 볼 수 있어.
1. 실제 객체의 주소
2. `control block`(제어 블록)에 있는 `(strong) reference count`를 비롯한 다양한 정보들

제어 블록에는 `(strong) reference count` 뿐만 아니라
해당 `shared_ptr`로 가리키는 객체의,
대표적으로 다음에 나올 `weak_ptr` 몇 개가 참조하고 있는지를 나타내는 `weak reference count` 같은
다른 정보도 함께 저장되고.


그리고 `use_count()` 함수는 이 값, `(strong) reference count`를 가져오는거야.

```cpp
std::shared_ptr<int> p1 = std::make_shared<int>(1000);
std::cout << p1.use_count() << '\n'; // 출력 : 1

std::shared_ptr<int> p2 = p1;
std::cout << p1.use_count() << '\n'; // 출력 : 2

{
    std::shared_ptr<int> p3 = p2;
    std::cout << p1.use_count() << '\n'; // 출력 : 3
}
// p3이 스코프를 벗어나서 소멸됨

std::cout << p1.use_count() << '\n'; // 출력 : 2
```
이렇게, 해당 `int` 객체를 소유하고 있는 `shared_ptr`의 수가 늘어날수록
`use_count()` 함수의 반환값이, `(strong) reference count`가 1씩 늘어나지.
처음에 생성된 `shared_ptr`인 `p1`에 `use_count()` 함수를 사용했는데도.

그리고 해당 객체를 소유하고 있는 `shared_ptr` 객체 1개가 스코프를 벗어나서 소멸하자,
`use_count()`로 반환된 `(strong) reference count`가 1 줄었지.

그러니까,
> shared_ptr이 복사될 때마다 (strong) reference count는 증가하고,
shared_ptr이 소멸될 때마다 (strong) reference count는 감소한다.

라고 할 수 있어.

그리고 `(strong) reference count`가 0이 되는 순간,
해당 객체의 소멸자가 호출되는거야.

물론 `control block`은 남아있을 수도 있는데,
이게 언제 남아있을 수 있는지는 다음에 나올 `weak_ptr`를 볼 때
다시 한번 살펴보자.

## 2-2. `shared_ptr`의 주의점

`shared_ptr`에는 확실한 몇 가지 주의점이 있고,
이 주의점들 중 몇 개를 보완하기 위해 다음에 나올 `weak_ptr`이 존재해.

그렇기 때문에, `shared_ptr`의 주의점을 확실히 알아야 
`weak_ptr`의 유용함을 알 수 있겠지?

### 2-2-1. control block을 공유해야 함

첫 번째 주의점으로, `shared_ptr`이 객체를 안전하게 관리할 수 있는 이유는 
모든 `shared_ptr`이 동일한 `control block`을 바라보며 `reference count`를 공유하기 때문이야.

하지만 우리가 실수로 동일한 객체에 대해 서로 다른 `control block`을 만들게 되면 대참사가 발생해.

```cpp
int* rawPtr = new int(100);
std::shared_ptr<int> sp1(rawPtr);
std::shared_ptr<int> sp2(rawPtr); // 위험! 새로운 Control Block이 또 생성됨
```

위 코드에서 `sp1`과 `sp2`는 같은 `int` 객체를 가리키지만, 서로 다른 `Control Block`을 가지게 돼.

`sp1`의 `(strong) reference count`는 `1`, `sp2`의 `(strong) reference count`도 `1`인 상태지.

만약 `sp1`이 스코프를 벗어나면? "어? 내가 마지막 소유자네?" 하고 `int` 객체를 삭제(`delete`)해버려.

그 후에 `sp2`가 스코프를 벗어나면? 이미 삭제된 주소를 또 삭제(`double delete`)하려고 시도하다가 프로그램이 바로 터져버려(`Segmentation Fault`).

```C++
std::shared_ptr<int> sp1(new int(100));
std::shared_ptr<int> sp2 = sp1; // sp1의 control block을 공유함.
```

이렇게 해야 `sp1`과 `sp2`가 하나의 `control block`을 함께 관리하게 되고, 
`(strong) reference count`가 제대로 `2`가 되면서 안전하게 소유권이 공유되는 거지.

### 2-2-2. 원자적 연산의 오버헤드와의 트레이드 오프

일단 제일 간단한 두 번째 주의점으로,
저렇게 여러 개의 `shared_ptr`이 참조하고 수정할 수 있는 `control block`도
레이스 컨디션이 발생할 수 있기 때문에,
`reference count` 등에 대한 연산도
변수 하나에만 레이스 컨디션의 발생이 묶여있는 경우에 상당히 좋은
원자적 연산(atomic operation)으로 `reference count`를 증감시켜.

하지만 예전에 적었던 글에서 말했던 것처럼
`atomic`에도 확실한 대가가, 소모되는 자원이 있어.
여기서는 데드락 위험보다는 
**캐시 락(해당 캐시 라인 잠그기)으로 인한 자원 소모**가 제일 큰 문제겠지.

`reference count`의 레이스 컨디션 발생 방지와
`shared_ptr`의 본래 목적인 자원 누수 방지라는 장점과,
캐시 락의 원리인 캐시 일관성을 보장하는 원리(MESI 프로토콜 등)로 인해서 발생한 오버헤드라는 단점이
트레이드 오프 되는거야.

물론 CPU에 따라 원자적 연산의 방식이 바뀔 수야 있지만,
원자적 연산이 수행된다는건 변하지 않아.

그래서 `shared_ptr`은 다른 스마트 포인터들에 비해 무거운 편이라,
포인터를 써야할 것 같을 때의 우선 순위는
```
포인터 안 쓰기 -> reference(T&) -> raw pointer(T*) -> unique_ptr -> shared_ptr -> weak_ptr 
```

정도라고 할 수 있어.

그래도 이 주의점은 트레이드 오프 뿐이지만..

### 2-2-3. 순환 참조(cyclic reference)문제

세 번째 주의점은 아예 shared_ptr의 본 목적인 자원 누수 방지라는 목적을
달성하지 못하게 될 수 있어.
**순환 참조(cyclic reference)**라고 하는건데,
쉽게 말하면 shared_ptr끼리 서로를 참조하고 있는거야.
근데 이러면 문제가,
- `reference count`가 0이 될 때 마지막으로 참조하고 있는 shared_ptr가 객체를 소멸시킴
- 하지만 서로가 서로를 참조하고 있으므로 `reference count`는 1에서 내려가지 않음
- 이건 프로그램이 종료될 때 까지도 지속됨
- 물론 프로그램이 종료되면 OS가 메모리를 회수해가지만, 실행 중의 문제는 확실함

이렇게 프로그램이 종료될 때 까지 계속 자원 누수가 발생할 수 있어.
그리고 게임 서버같은건 일반적인 컴퓨터에 비해 훨씬 종료 빈도가 낮을 수 밖에 없으니,
이 문제는 치명적이지.

뭔가 단순히 텍스트만으로 설명하긴 어려우니까, 예시를 한번 들어볼게.
```cpp
class A {
    std::shared_ptr<B> a;
};

class B {
    std::shared_ptr<A> b;
};
```
이렇게, 서로가 서로의 객체를 shared_ptr로 참조하고 있다면,
양쪽의 shared_ptr의 reference count는 1 이하로 내려갈 수 없겠지?
적어도 서로가 서로를 참조하고 있고,
각각의 shared_ptr의 종료 조건은 reference count가 0이 되는건데..
마치 데드락?
이걸 그래프로 그려본다면, 뭔가 데드락이 발생했을 때의 자원 할당 그래프같이,
양쪽 클래스의 shared_ptr가 꼬리에 꼬리를 물고 상대의 참조가 끝나기를 기다리는 느낌이겠지..

그리고 이 문제를 해결하기 위해, 
바로 다음 챕터의 주제인 `weak_ptr`가 있어.

### 2-2-4. 객체를 소유하지는 않고 딱 접근만 하고 싶을때 생기는 문제

이게 무슨 개소리인가 싶지만, 
정말 쉽게 말하면
> 해당 객체에 접근은 하고 싶지만, 
shared_ptr들이 소유하고있는 해당 객체의 생명 주기에 간섭하고 싶지 않아서 
shared_ptr로 소유하고 싶지 않을 때

라는거야.

이런건 앞에서 말한 순환 참조와도 연관이 있는데, 
두 객체 안의 `shared_ptr`이 서로를 참조하고 있어서
순환 참조가 발생하는데,
그 중 한 객체는 굳이 상대 객체를 소유할 필요가 없고, 
딱 접근만 해도 되는 경우가 있을 수 있어.

그러니까 정확히 표현하자면, 
굳이 참조하는 데에 `reference count`를 올리지 않고,
`shared_ptr`로 소유하지 않고 딱 참조만, 접근만 하는게 좋은 경우가 있을 수 있다는거야.

---

예를 들어, 멀티 클라이언트 서버를 생각해보자.

서버 객체는 접속 중인 클라이언트 세션들을 관리하기 위해
각 `ClientSession` 객체를 `shared_ptr`로 가지고 있을 수 있어.

```cpp
class Server {
private:
    std::vector<std::shared_ptr<ClientSession>> clients;
};
```

이건 자연스러워.
서버가 클라이언트 세션 목록을 관리하고,
클라이언트 세션 객체의 수명에도 관여하기 때문이야.

그런데 반대로 `ClientSession` 객체도 서버 객체에 접근해야 할 수 있어.

예를 들어,

- 접속 종료 시 서버에게 자신을 목록에서 제거해달라고 요청하거나
- 나중에 채팅 서버로 확장되었을 때 서버에게 브로드캐스트를 요청하거나
- 서버의 공용 설정이나 로그 시스템에 접근해야 할 수 있어

이런 경우 `ClientSession`은 `Server`를 알고 있어야 해.

하지만 그렇다고 해서 `ClientSession`이 `Server`를 `shared_ptr`로 소유하는 것은 이상해.
```cpp
class ClientSession {
private:
    std::shared_ptr<Server> server; // 문제가 될 수 있음
};
```
왜냐하면 `ClientSession`은 서버를 **'사용'**하는 객체일 뿐이지,
서버의 수명을 책임지는 객체가 되어서는 안되기 때문이야.

서버는 프로그램 전체 또는 서버 실행 흐름 전체를 관리하는 상위 객체고,
클라이언트 세션은 클라이언트 하나의 연결이 유지되는 동안만 살아있는 하위 객체에 가까워.

즉 관계를 말로 표현하면,

- `Server`는 `ClientSession`을 소유할 수 있다.
- 하지만 `ClientSession`은 `Server`를 소유하면 안 된다. 
즉, `Server`의 수명에 영향을 끼치면 안된다.
- `ClientSession`은 `Server`에 단순히 접근만 하면 된다.
- 하지만 `ClientSession`은 `Server`가 소멸되지 않은 경우에만 `Server`에 접근해야한다.

이런 상황에서 `ClientSession`이 `Server`를 `shared_ptr`로 소유하면,
쓸데없이 서버 객체의 `reference count`를 증가시키게 되는거야.

더 심한 경우에는

```cpp
Server -> shared_ptr<ClientSession>
ClientSession -> shared_ptr<Server>
```

처럼 서로가 서로를 소유하는 구조가 되어
순환 참조가 발생할 수 있어.

그래서 이런 경우에는 `ClientSession`이 서버를 `shared_ptr`로 소유하는 대신,
'소유하지 않는 접근'을 하는 편이 더 자연스럽겠지.

---

`reference count`를 올리지 않는다면, 
상대 객체의 생명 주기에는 영향을 주지 않아.
`reference count`가 0이 되어야 상대 객체가 소멸할텐데,
어차피 우리 객체가 상대 객체에 `shared_ptr`로 `reference count`를 올리지 않고
그냥 단순히 접근만 하니까,
우리 객체가 상대 객체의 생명 주기에 영향을 주지 않는거지.
그래서 순환 참조가 발생하지는 않겠지.

근데 여기까지만 들어보면 그냥 raw pointer(원시 포인터, void*)로 접근해버려도 될 것 같잖아?
딱히 소유권 안 가지고, reference count 안 올리고..

그런데 여기서 문제가 있는데,
원시 포인터의 문제점 중 하나인
댕글링 포인터, 즉 이미 사라진 객체를 참조하려고 하는 문제가 생길 수 있어.

이미 `reference count`가 0이 되어서 객체가 사라졌는데,
우리 객체는 그 객체가 사라져있다는걸 모르니까
그 객체에 접근하려다 **쓰레기 값을 읽어오거나, 세그폴트(segmentation fault)가 발생**할 수 있는거지.

## 3. Class `weak_ptr`

바로 전에 **cyclic reference(순환 참조)**와 
**객체를 소유하지 않고 접근만 해야할 때의 문제**를 이야기했었지?
그 문제를 해결하기 위한 클래스가 `weak_ptr` 클래스야.

`weak_ptr` 클래스는 객체에 대한 접근은 허용하지만, 
**해당 객체를 소유하지는 않아.**'
그리고 `shared_ptr`로 참조하고 있는 객체가 소멸되었을 때에
소멸된 객체를 가리키지 못하도록 무효화돼.

`weak_ptr`은 비어 있는 상태로도 만들 수 있고,
다른 `weak_ptr`로부터 복사할 수도 있어.

하지만 어떤 객체를 관찰하는 `weak_ptr`을 만들려면,
보통 그 객체를 소유하고 있는 `shared_ptr`로부터 생성해.
왜냐하면 `weak_ptr`은 실제 객체의 생존 여부를
`shared_ptr`들이 공유하는 `control block`을 통해 확인하기 때문이야.

`weak_ptr`은 해당 객체를 소유하지는 않지만, `(strong) reference count`를 올리지는 않지만,
해당 객체를 가리키는 `shared_ptr`들의 제어 블록에는 접근해서
해당 객체가 정상적으로 남아있는지, 소멸되지는 않았는지 확인하고 해당 객체에 접근해.

근데 여기서 궁금할 수도 있는게,
> `shared_ptr`들이 소유하던 실제 객체가 소멸되었는데, 
`weak_ptr`은 어떻게 `control block`에 접근해서 그 객체가 소멸되었는지 알 수 있을까?

`weak_ptr`이 `shared_ptr`들이 소유하고 있는 객체를 참조하는 동작은
`(strong) reference count`와는 별개인
`weak reference count`를 올려.

`(strong) reference count`가 0이 되어서 `weak_ptr`이 참조하고 있는 객체가 소멸되었다고 해도,
`weak reference count`가 0이 아니라면 제어블록까지는 소멸되지 않아.

`weak_ptr`들이 `control block`을 보고 해당 객체가 소멸되었는지 알 수 있어야 하니까.
아무런`shared_ptr`도 `weak_ptr`도 해당 객체를 참조하지 않을 때,
더이상 `control block`이 필요없게 될 때,
즉, 
> 이미 `(strong) reference count`가 0이 되어서 객체는 소멸했어도
`weak reference count`가 0이 될 때까지 `control block`은 살아있게 돼.

- 객체의 수명 결정 = `(strong) reference count`
- control block의 수명 결정 = `weak reference count`

그리고 댕글링 포인터 문제를 방지하기 위해서,
`weak_ptr`은 참조하고 있는 객체가 소멸되었을 때,
자동으로 해당 `weak_ptr`을 무효화시켜.
물론 `weak_ptr` 객체 자체가 소멸되는건 아니고,
그저 참조할 객체가 사라져 expired 상태가 될 뿐이야.
그냥 못 쓰는 상태라고 생각하면 편해.


정말정말 간단하게 요약하자면,
> 참조할 객체의 수명에 영향을 주지 않는데, 
딱 접근만은 할 수 있고,
객체가 사라졌을 때는 안전한 스마트 포인터

라고 할 수 있어.

좀 더 복잡하게 설명해보자면,
> `shared_ptr`가 참조하고 있던 객체를 참조하고 싶지만,
해당 객체의 `(strong) reference count`를 증가시켜서 해당 객체의 수명에 영향을 주고싶지는 않은데,
그저 접근하는 것이 필요한 경우에 사용하고,
`shared_ptr`가 참조하고 있던 객체가 소멸되었을 때에는 
댕글링 포인터 문제가 발생하지 않게 해당 객체에 접근이 막히는
스마트 포인터

라고 할 수 있겠지.

## 3-1. `weak_ptr`의 사용법과 동작

일단, `weak_ptr`은 `*`나 `->`같은 역참조 연산자로
해당 포인터가 가리키는 객체에 접근할 수 없어.

앞에서 말했듯이, `weak_ptr`은 객체를 소유하지 않기 때문에,
이미 객체가 소멸되었을 가능성이 있어서 객체의 존재 여부를 항상 확인해야해.

그래서 역참조 연산자로 접근하면 이미 객체가 소멸되었을 때 
댕글링 포인터 문제가 발생할 수 있기 때문이야.

```cpp
std::shared_ptr<int> sp = std::make_shared<int>(1000);
std::weak_ptr<int> wp = sp;
```
이렇게 `shared_ptr`로부터 `weak_ptr`을 생성할 수 있어.

그런데 `weak_ptr`이 가리키는 객체에 접근하려면 어떻게 해야하냐면,
```cpp
auto locked = wp.lock();
```
이렇게 `lock()` 함수를 사용해야해.

만약 해당 객체가 아직 살아있다면,
`lock()` 함수는 해당 객체를 가리키는 `shared_ptr`을 반환해.

반대로 객체가 이미 소멸되었다면,
`lock()` 함수는 비어있는 `shared_ptr`을 반환해.

그러니까, 정말 쉽게 `weak_ptr`의 동작을 설명하자면
> 객체에 접근 가능하다면 잠시동안 소유하는 용도의 shared_ptr을 얻어서
반환된 shared_ptr로 잠시동안 해당 객체에 대한 소유권을 얻어서
해당 객체에 접근한다.

그래서 보통은,
```cpp
if (auto locked = wp.lock()) {
    std::cout << *locked << '\n';
}
else {
    std::cout << "이미 객체가 소멸됨\n";
}
```
이렇게 사용돼.

`weak_ptr` 객체인 `wp`에 `lock()` 함수를 사용해서
제대로 `shared_ptr`이 반환이 되었다면 
해당 `shared_ptr`을 통해 잠시동안 객체에 접근한 후 
해당 `shared_ptr`을 무효화하는 식으로.

## 3-2. `weak_ptr`을 실제 구조에 적용해보기

앞에서 말했던 Server-ClientSession 구조를 `weak_ptr`을 사용해서 구현해봤어.
여기서 중요한건,
- `Server`는 `ClientSession`을 소유해야 한다
- 하지만 `ClientSession`은 `Server`를 소유하면 안된다
- 대신 접근만 할 수 있어야한다.

이렇게야.

실제 코드로는,
```cpp
#include <iostream>
#include <memory>
#include <vector>

class Server;

class ClientSession {
private:
    std::weak_ptr<Server> server_wp;
public:    
    ClientSession() {
        std::cout << "[ClientSession]클라이언트 세션 생성\n";
    }

    ~ClientSession() {
        std::cout << "[ClientSession]클라이언트 세션 종료\n";
    }

    void ConnectToServer(std::shared_ptr<Server> s) {
        server_wp = s;
    }

    void SendToServer(const std::string& msg);    

};

class Server : public std::enable_shared_from_this<Server> {
private:
    std::vector<std::shared_ptr<ClientSession>> clients;
public:
    Server() {
        std::cout << "[Server]서버 객체 생성\n";
    }

    ~Server() {
        std::cout << "[Server]서버 객체 소멸\n";
    }

    void ClientAdd(std::shared_ptr<ClientSession> client) {
        clients.push_back(client);
        client->ConnectToServer(shared_from_this());
    }

    void RecvFromClient(const std::string& msg) {
        std::cout << "[Server]RecvFromClient : " << msg << '\n';
    }
};

void ClientSession::SendToServer(const std::string& msg) {
        if (auto locked_ptr = server_wp.lock()) {
            locked_ptr->RecvFromClient(msg);
        }
        else {
            std::cout << "[ClientSession] 서버 닫힘\n";
        }
    }

int main() {
    std::shared_ptr<ClientSession> outside_client;

    {
        std::shared_ptr<Server> server = std::make_shared<Server>();
        std::shared_ptr<ClientSession> client = std::make_shared<ClientSession>();
        server->ClientAdd(client);
        client->SendToServer("대충 메시지임");
        outside_client = client; // client가 소유하고 있는 ClientSession 객체에 대한 소유권 공유
    }
    // 스코프를 벗어나서 server shared_ptr 객체가 소멸됨에 따라 Server 객체도 소멸됨

    outside_client->SendToServer("이 메시지가 출력될까?"); 
    // 출력 되지 않음. weak_ptr이 쥐고 있는 Control block의 Server 객체가 소멸되었기 때문에 server_wp.lock()이 nullptr 반환

    return 0;
}
```
이렇게 구현할 수 있어.

이 코드를 간단하게 설명하자면,
> `Server` 객체는 `vector<shared_ptr<ClientSession>>`으로 `ClientSession` 객체들을 소유하고,
`ClientSession` 객체는 `weak_ptr<Server>`로 `Server` 객체를 소유하지 않고 참조만 한다.

> 그리고 `ClientSession`에 `Server` 객체의 `shared_ptr`은 
Server에서 enable_shared_from_this를 활성화한 후 shared_from_this() 함수로
해당 Server 객체의 shared_ptr을 ClientSession 객체에 넘겨주는 방식으로
weak_ptr를 초기화해준다.

> ClientSession 객체가 Server 객체에 메시지를 보낼 수 있고,
메시지를 보낼 때는 weak_ptr을 lock() 함수로 Server 객체 소멸 여부를 확인한 후
Server 객체의 shared_ptr이 반환되면 
해당 shared_ptr로 Server 객체의 메시지 출력 함수(RecvFromClient())를 실행한다.
그리고 해당 shared_ptr은 스코프에서 벗어나서 자동적으로 소멸된다.

라고 할 수 있어.

`main()` 함수 내부의 동작은, 
```cpp
int main() {
    std::shared_ptr<ClientSession> outside_client;

    {
        std::shared_ptr<Server> server = std::make_shared<Server>();
        std::shared_ptr<ClientSession> client = std::make_shared<ClientSession>();
        server->ClientAdd(client);
        client->SendToServer("대충 메시지임");
        outside_client = client; // client가 소유하고 있는 ClientSession 객체에 대한 소유권 공유
    }
    // 스코프를 벗어나서 server shared_ptr 객체가 소멸됨에 따라 Server 객체도 소멸됨

    outside_client->SendToServer("이 메시지가 출력될까?"); 
    // 출력 되지 않음. weak_ptr이 쥐고 있는 Control block의 Server 객체가 소멸되었기 때문에 server_wp.lock()이 nullptr 반환

    return 0;
}
```
`Server` 객체가 살아있을 때와 소멸되었을 때 `SendToServer()` 함수를 호출해서
`weak_ptr`이 `Server` 객체가 소멸되어있다면 서버 닫힘을 출력하는 것을 확실히 보기 위한 코드라고 요약할 수 있어.

```
[Server]서버 객체 생성
[ClientSession]클라이언트 세션 생성
[Server]RecvFromClient : 대충 메시지임
[Server]서버 객체 소멸
[ClientSession] 서버 닫힘
[ClientSession]클라이언트 세션 종료
```

1. 다른 객체들의 스코프 밖에 있는 `outside_client` `shared_ptr`이 생성됨

2. `server` `shared_ptr`과 해당 `shard_ptr`이 소유권을 가진 `Server` 객체가 생성됨
3. `client` `shared_ptr`과 해당 `shared_ptr`이 소유권을 가진 `ClientSession` 객체가 생성됨
4. `server` `shared_ptr`이 `client shared_ptr`을 `clients` `vector`에 추가 후 
`ClientSession` 객체의 `server_wp` `weak_ptr`을 `Server` 객체의 `shared_ptr`을 사용해서 초기화
5. `ClientSession` 객체의 `SendToServer()` 함수가 호출되어서
`Server` 객체의 `RecvFromClient()` 함수가 실행되어 메시지가 출력됨
6. `client` `shared_ptr`이 소유하고 있는 `ClientSession` 객체에 대한 소유권을 
`outside_client` `shared_ptr`과 공유
7. 스코프를 벗어나서 
`server` `shared_ptr`과 `Server` 객체(`(strong) reference count` = 0), 
`client` `shared_ptr`이 소멸됨. 
`ClientSession` 객체는 
아직 `outside_client` `shared_ptr`이 참조하고 있기 때문에(reference count = 1) 
소멸되지 않음.
8. `Server` 객체가 소멸되어있기 때문에 `outside_client` `shared_ptr`이
소유하고 있는 `ClientSession` 객체의 `SendToServer()` 함수를 호출해도
`lock() == nullptr`이기 때문에, 서버 객체가 소멸되었기 때문에
서버 닫힘 메시지 출력
9. 프로그램 동작이 끝나서 `outside_client` `shared_ptr`도 소멸,
`reference count`가 0이 되어서 `ClientSession` 객체도 소멸.

## 4. 객체의 소유권이란

앞에서의 `shared_ptr`과 `weak_ptr`에서 계속 나온 단어가 있었지.
`소유권 - ownership`
지금까지 봤다면 `shared_ptr`과 `weak_ptr`은 
결국
> 이 객체를 여러 곳에서 참조하는 포인터를 사용할 때,
어느 포인터가 해당 객체를 delete해서 자원 누수를 막을 것인가?
즉, 어떤 포인터가 해당 객체의 생명 주기(lifetime)에 간섭하는
**소유권**을 가질 것인가?
- 쉽게 말해 
여러 곳에서 참조할 때 어떤 타이밍에 객체를 소멸시켜서 자원 누수를 막을 것인가.
- `shared_ptr`로 해결 가능

> 만약 해당 객체를 delete하는 전체적인 흐름에,
해당 객체의 생명 주기에 영향을 주고 싶지 않다면,
**소유권**을 가지고 싶지 않다면,
어떻게 할 것인가?
- 쉽게 말해
`shared_ptr`을 사용할 때 객체가 소멸되는 타이밍에 영향을 주지 않고 접근만 하고싶을 때 
어떻게 할 것인가.
- `weak_ptr` + `raw pointer`(원시 포인터도 해당은 됨. 하지만..)로 `shared_ptr`들이 소유하는 객체를 참조하는 것으로 해결 가능

> **소유권**을 가지지 않은 채로 접근만 할 때의
객체 수명에 영향을 줄 수 없기 때문에 생기는,
이미 소멸된 객체에 접근할 때에 생기는,
댕글링 포인터 문제를 어떻게 해결할 것인가?
- 쉽게 말해
어떤 `shared_ptr`도 더이상 소유하지 않아서 이미 소멸된 객체에 접근할 때에 생기는
댕글링 포인터 문제를 어떻게 해결할 것인가?
- `weak_ptr`만이 아직 살아있는 `control block`을 보고 
해당 객체의 소멸 여부를 판단하는 것으로 해결 가능


근본적으로는 이 문제들의 답이라고 생각해. 

전부 다 소유권이 나오지.
- 소유권을 가질 것인가?
- 소유권을 가지고 싶지 않다면,
- 소유권을 가지지 않은 채로 접근만 할 때의

그리고 그게 전부 핵심이고..

처음에 스마트 포인터를 사용하는 이유에서 말했던
- 자원 누수
- 댕글링 포인터

전부 
- 소유권을 명확히 하기 
  - 마지막으로 해당 객체를 소유한 `shared_ptr`이 `delete`를 해줌으로써 자원 누수 방지
- 소유권을 가지지 않은 포인터가 접근할 때에 해당 객체가 살아있는지 확인하기 
  - `weak_ptr`이 `shared_ptr`들의 `control block`(특히 `reference count`)를 보고 
  해당 객체에 접근할 수 없을 때는 접근하지 않는 식으로 댕글링 포인터 방지

이렇게
해결할 수 있다는거야.

다 객체의 소유권에 관련있지.

결국 뒤에서 나올 스마트 포인터인 `unique_ptr`도
> 객체의 소유권을 어떻게 할 것인가.

즉,
> 누가 해당 객체를 delete할 것인가.
누가 해당 객체의 수명에 책임을 질 것인가.


부터 시작해.

그냥 모든 스마트 포인터들이 이 주제에서부터 시작하는거야.

## 5. Class `unique_ptr`

이 `unique_ptr`라는 스마트 포인터는
`unique - 고유한`이라는 단어에 맞게,
다른 스마트 포인터들을 제외하고(exclusive) 
해당 `unique_ptr`만 단독으로 객체를 소유(exclusive ownership)하는,
해당 `unique_ptr`만 객체의 수명에 책임을 지는,
해당 `unique_ptr`만 해당 객체를 delete할 수 있는
스마트 포인터야.

쉽게 말해서 **자신이 가리키는 객체의 유일한 소유자가 되는 포인터**라고 할 수 있어.
그래서 **`unique_ptr`이 가리키는 객체는 해당 `unique_ptr`만 소유자**가 될 수 있어.

그리고 `unique_ptr`이 어떤 객체를 소유하고 있다면,
그 객체는 해당 `unique_ptr`이 소유권을 잃거나 소멸되기 전까지 살아있어.

하지만 `unique_ptr` 객체 자체가 살아있다고 해서 항상 객체를 소유하고있는건 아니야.
`std::move()`, `reset()`, `release()` 이후에는 비어있는 `unique_ptr`이 될 수도 있어.
이런 연산들은 사용법 볼 때 보자.

그리고 `unique_ptr`은 `shared_ptr` / `weak_ptr`과 다르게
딱히 해당 객체의 소유자 갯수 / 참조 갯수를 셀 필요가 없어서
`control block`이 필요하지 않고,
`shared_ptr`보다 훨씬 가벼워지지.

어쨌든, 
`unique_ptr`은 어떻게 보면,
가장 기본적인 스마트 포인터로 보여.

객체의 소유권을 나타내는
스마트 포인터의 본 목적에 아주 충실해.

`unique_ptr` 클래스는 `C++11` 이전의 `auto_ptr` 클래스를 대체한 클래스로,
이전의 `auto_ptr`은 에러가 `unique_ptr`보다 에러도 많았고, 사용하기도 불편해서
더 편리한 `unique_ptr`가 나왔어.

책에서는 먼저 **Purpose of Class `unique_ptr`(`unique_ptr`의 목적)** 챕터가 있는데,
여기서 설명하는건 결국
> 예외가 발생했을 때 기존의 raw pointer로는 획득한 자원을 안전하게 해제하려면
코드가 복잡해지고 중복 코드도 많아지기에
소멸될 때 자동으로 자원들을 해제해주는 `unique_ptr`가 등장했다.

(물론 힙 영역에 동적 할당한 객체도 결국 메모리라는 자원을 할당받는거고.)

> 그리고 당연히 스코프를 벗어날 때, 함수를 벗어날 때
`unique_ptr`은 소멸된다.

라고 이해했어.

```cpp
void func() {
	ClassA* ptr = new ClassA;
    
    try{
    	// 함수 내부 실행
    }
    catch (...){
    	delete ptr;
        throw;
    }
    
    delete ptr;
}
```
이 코드에서는 객체가 하나지만, 
두 번째 객체 이상의 객체들을 힙 영역에 할당받게 된다면
`try-catch` 문은 훨씬 복잡해지겠지.

`unique_ptr`을 사용하면
```cpp
#include <memory>

void func() {
	std::unique_ptr<ClassA> up1(new ClassA);
    std::unique_ptr<ClassA> up2 = std::make_unique<ClassA>(); // C++14 이후 가능
    
    // 함수 내부 실행
}
// 예외가 발생하거나 함수가 종료되면 스코프를 벗어나게 되니 자동으로 classA 객체는 delete
```
이렇게, 단순히 선언만 해놓고 나머지 신경을 안 써도,
- 함수 실행 도중에 예외가 발생하든
- 함수가 종료되든

별다른 코드 없이 안전하게 unique_ptr이 가리키는 classA 객체를 메모리에서 해제해주지.

## 5-1. `unique_ptr`의 사용법과 동작

`unique_ptr`은 일반적인 raw pointer(`void*` 같은 원시 포인터)와 
어느정도는 비슷한 조작을 할 수있어.

- *나 ->로 객체를 역참조
```cpp
(*up)[0] = 'A';
up->push_back('B');
```

그리고 raw pointer와의 차이점은,
포인터 연산이 안된다는 점.
```cpp
up + 1 = 'C'; // 이런거 안됨
```

물론 `shared_ptr`처럼 초기화 할 때도 생성자에 `explicit` 키워드가 붙어있어서
```cpp
std::unique_ptr<ClassA> up1 = new ClassA; // 이렇게는 불가(암시적 형 변환 발생)
std::unique_ptr<ClassA> up2(new ClassA); // 이렇게는 가능(암시적 형 변환 발생하지 않음)
```

그리고 앞의 `shared_ptr`랑 `weak_ptr`처럼
빈 `unique_ptr`도 정의될 수 있어.
기본 생성자(매개변수가 없는 생성자)로 초기화된거지.
```cpp
std::unique_ptr<int> up; // 아무런 객체도 가리키지 않는, 빈(nullptr) unique_ptr
```

그리고 이렇게 빈 `unique_ptr`을 직접, 
이미 객체를 가리키고 있는 `unique_ptr`을 빈 `unique_ptr`로 만들려면
```cpp
up = nullptr;
up.reset();
```
이렇게 만들어줄 수 있어.

왜 이런걸 이렇게 따로 설명하는지 궁금할 수 있는데,
이건 다음 파트인 `unique_ptr`의 소유권 이동 파트에서 나올거라서 그래.

그리고 추가적으로는
```cpp
std::unique_ptr<int> up(new int);

int* a = up.release();
```
이렇게 `release()` 함수로 해당 객체에 대한 소유권을 포기할 수도 있다는거?
이거는 `reset()` 함수와 다른 점이, 
`reset()` 함수는 아예 해당 `unique_ptr`의 값을 지워버린다면,
`release()` 함수는 해당 `unique_ptr`이 소유하고 있는 객체의 소유권을 포기,
즉, 그저 다른 raw pointer 등으로 해당 `unique_ptr`의 값을 넘겨줄 수 있다는거야.
`release()` 함수의 반환값도 raw pointer이고.

그런데 결국 raw pointer를 반환하기 때문에,
여기서부터는 직접 해당 raw pointer을 delete해줘야 해. 
직접 자원을 해제해야한다는거지.
안 그러면 메모리 누수가 발생할 수 있으니 `release()` 함수를 사용할 때는 조심해야해.

그리고 `shared_ptr`이나 `weak_ptr`처럼 
`get()` 함수로 실제 소유하고 있는, 참조하고 있는 객체의 메모리 값을 얻어올 수 있는 정도야.

### 5-1-1. `unique_ptr`의 소유권 이동 - move semantics

맨 앞에서 `unique_ptr`에 대해 간단히 설명했을 때

- 소유권(ownership)을 공유하지 않음. 단독으로(exclusive) 소유
- 한 스마트 포인터만 하나의 객체에 대한 소유권을 가질 수 있음
- 하지만 소유권 이동(transfer)은 가능

이라고 했지.

이 파트는 실제로 어떻게 단독으로 객체를 소유하는 `unique_ptr`에서
소유권 이동을 실제 C++ 문법 상으로 어떻게 하는지,
그리고 내부적으로는 어떤 일이 일어나는지에 대한 이야기야.

알다시피,
```cpp
std::unique_ptr<int> up1(new int(10));
std::unique_ptr<int> up2 = up1; // 에러 발생
```
`unique_ptr`은 같은 객체를 공유 소유할 수 없고,
단독(exclusive)으로 소유해야해.

근데 그러면 어떻게 `unique_ptr`의 소유권을 옮길 수 있을까?
복사 생성자같은거 다 막혀있을텐데..

실제로 `unique_ptr`은 일반적인 copy semantics(동작 규칙)로는 당연히 복사가 막혀있어.

그래서, 다른 스마트 포인터로 소유권을 옮길 때는
move semantics(동작 규칙)에 따라서 소유권을 이동시켜줘야해.
`C++11`부터 새로 나온 semantics(동작 규칙)임.

```cpp
std::unique_ptr<int> up1(new int);

std::unique_ptr<int> up2 = up1; // 에러. copy semantics 유효하지 않음.

std::unique_ptr<int> up3(std::move(up1)); // 이렇게 std::move를 사용해서 소유권을 이동할 수 있음
```

내부적으로는, 그러니까 semantics는
내가 예전에 `ClientSocket` 객체를 구현할 때 사용했던 
이동 생성자와 비슷해.
```cpp
// 이동 생성자
    ClientSocket(ClientSocket&& other) noexcept : client_sock(other.client_sock) { // noexcept = 이 함수는 예외를 발생시키지 않는다고 컴파일러에게 알려주기. 그래서 이동 최적화.
        other.client_sock = INVALID_SOCKET;
    }
```
이런 생성자였는데,
동작을 살펴보자면 

1. 복사가 금지된 해당 객체를 `r-value reference`로 가져오기
2. 가져온 객체를 현재 객체로 복사하기
3. 가져온 객체(원래 객체)는 무효화하기

이렇게 동작해.

실제로도 `unique_ptr`은 이런 move semanstic이 적용되어있어.
(물론 로직만 같음)

```cpp
std::unique_ptr<int> up1(new int);

std::unique_ptr<int> up2;

up2 = std::move(up1); // up1에서 up2로 int 객체의 소유권 이동
```
이 코드에서도 `up1`을 무효화, 그러니까 `nullptr`로 바꾸고
해당 `int` 객체의 소유권을 `up2`로 이동하는 식으로 
소유권의 이동이 일어나.

```cpp
std::unique_ptr<int> up1(new int);

std::unique_ptr<int> up2(new int);

up2 = std::move(up1); // up2의 원래 객체가 지워지며 up1에서 up2로 int 객체의 소유권 이동
```
그리고 이렇게 이동할 위치의 스마트 포인터(`up2`)에 같은 타입의 객체를 이미 소유하고있으면,
이동시 해당 객체는 소멸되고 새 객체(`up1`로부터 이동한 객체)를 소유하게 돼.

## EX. 실전 사용 요약

어떤 스마트 포인터를 쓰는게 좋을까?
```
1. 그냥 값으로 둘 수 있나?
   - 가능하면 값 멤버 / 지역 변수

2. 소유하지 않고 잠깐 쓰기만 하나?
   - 반드시 존재하면 T&
   - 없을 수도 있으면 T*

3. 소유권이 필요한가?
   - 소유자가 하나면 unique_ptr
   - 소유자가 여러 개면 shared_ptr

4. shared_ptr 객체를 소유하지 않고 관찰만 해야 하나?
   - weak_ptr
```

사용 우선순위
```
포인터 안 쓰기 -> reference(T&) -> raw pointer(T*) -> unique_ptr -> shared_ptr -> weak_ptr 
```