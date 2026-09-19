## 0. 서론

지금까지는 대체로 정수만 다뤘지?

- 레지스터 종류
- 데이터 이동 명령어
- 산술 연산 명령어
- 조건 코드 생성 명령어(비교 명령어)
- 프로시저의 인자 전달

전부 값이 정수라는 가정 하에 배웠으니까.

이번에는 정수 말고, 실수.
즉 부동소수점 데이터들이

- 어떤 레지스터에 담기며
- 어떤 데이터 이동 명령어를 사용하며
- 어떤 산술 연산 명령어를 사용하며
- 어떤 조건 코드 생성 명령어를 사용하며
- 어떻게 프로시저에 인자로 전달되는지

를 배울거야.

3.11 : Floating-Point Code라는 이름에 맞게.

- 3.11.1 : Floating-Point Movement and Conversion Operations - 부동소수점 이동 / 변환 명령어
- 3.11.2 : Floating-Point Code in Procedures - 프로시저에서의 부동소수점 코드
- 3.11.3 : Floating-Point Arithmetic Operations - 부동소수점 산술 연산 명령어
- 3.11.4 : Defining and Using Floating-Point Constants - 부동소수점 상수의 정의 / 사용
- 3.11.5 : Using Bitwise Operations in Floating-Point Code - 부동소수점 코드에서 비트 연산 명령어 사용하기
- 3.11.6 : Floating-Point Comparison Operations - 부동소수점 비교 명령어

이 정도를 배울거야.

생각보다 별거 아니야.
그냥 사용하는 레지스터와 사용하는 명령어가 정수 관련 명령어와 이름만 다를 뿐,
비슷한 명령어들이 많아.

그리고 일단 선수 지식으로,
부동소수점 데이터들은
정수형 데이터들과 다른 레지스터 집합(Register set)을 사용해.

흔히 `%xmm` 레지스터라고 하는데,
이것도 한 레지스터의 사용하는 비트 수의 따라 이름이 달라.

128비트를 사용하는 레지스터들은 `%xmm0` ~ `%xmm15`가 있고,
256비트 전체를 사용하는 레지스터들은 `%ymm0` ~ `%ymm15`가 있어.

정수 레지스터와 다르게 이름이 참 간단하지?

그리고 `0 ~ 8`번 레지스터는 인자 전달 용도로도 사용할 수 있고,
`0`번 레지스터는 반환 값을 저장할 레지스터로도 사용할 수 있어.
나머지 `%xmm` 레지스터들은 전부 Caller-saved 레지스터고.

## 1. Floating-Point Movement and Conversion Operations - 부동소수점 이동 / 변환 명령어

### 정리

1. 부동소수점 이동 명령어
    - `M(n)` : `n`바이트의 메모리 공간
    - `X` : `xmm` 레지스터
    - 메모리 to 메모리 이동은 안됨.
    - 
    |명령어|설명|
    |---|---|
    |`vmovss X/M(32), X/M(32)`|스칼라 단일(single) 정밀도 이동|
    |`vmovsd X/M(64), X/M(64)`|스칼라 이중(double) 정밀도 이동|
    |||
    |`vmovaps X/M(128), X/M(128)`|패키지 단일 정밀도 이동(정렬됨)|
    |`vmovapd X/M(128), X/M(128)`|패키지 이중 정밀도 이동(정렬됨)|
    - 데이터 한 개만 가져오는 스칼라 전용 명령어와
    데이터 여러개를 통째로 가져오는 패키지 전용 명령어가 분리되어있다.
    - 패키지 전용 명령어는 사용하는 레지스터 크기에 따라 가져오는 비트 수가 다르다.
    - 중요한게,
    `vmovaps` / `vmovapd` 명령어(패키지 전용 명령어)들은
    데이터가 정렬되어있다는 가정 하에 메모리에서 데이터들을 가져온다.
      - 메모리에서 데이터들 가져올 때, 주소가 정렬되어있지 않다면 예외가 발생한다.
      - 정렬은 16바이트 정렬이다.
      - 물론 레지스터끼리 데이터를 옮길 때는 잘못된 정렬로 인한 예외가 발생하지 않는다.
      
2. 부동소수점 변환 명령어
    - 부동소수점 수를 정수로, 정수를 부동소수점 수로 변환하는 명령어들이다.
    - 이름이 많이 더러운데, 이렇게 해석하면 된다.
      - (예시) `vcvttss2si`
      `cvt` : convert(변환)
      `t` : truncate(절삭)
      `ss` : scalar single-precision(스칼라 float)
      `2` : to
      `si` : single Integer
      즉, scalar float -> single integer로 변환하되 소수점 이하는 버린다는 뜻이다.
    - `R(n)` : `n`비트의 정수 레지스터
    -
    |명령어|뜻|
    |---|---|
    |`vcvttss2si X/M(32), R(32)`|convert with truncation single precision to integer|
    |`vcvttsd2si X/M(64), R(32)`|convert with truncation double precision to integer|
    |`vcvttss2siq X/M(32), R(64)`|convert with truncation single precision to quad word integer|
    |`vcvttsd2siq X/M(64), R(64)`|convert with truncation double precision to quad word integer|
    |||
    |`vcvtsi2ss M(32)/R(32), X, X`|convert integer to single precision|
    |`vcvtsi2sd M(32)/R(32), X, X`|convert integer to single precision|
    |`vcvtsi2ssq M(64)/R(64), X, X`|convert quad word integer to single precision|
    |`vcvtsi2sdq M(64)/R(64), X, X`|convert quad word integer to double precision|
    |||
    |`vcvtss2sd M(32)/X, X, X`|convert single precision to double precision|
    |`vcvtsd2ss M(64), X, X`|convert single precision to double precision|
    - 오퍼랜드가 세 개인 명령어(정수 to 부동소수점 명령어)들은
      - 첫 번째 오퍼랜드 : 변환할 정수(source)
      - 두 번째 오퍼랜드 : 무시해도 됨(진짜로 무시해도 됨)
      - 세 번째 오퍼랜드 : destination
      - 의 형식을 가진다.
    - 정수 to 부동소수점, 부동소수점 축소 / 확대 명령어는 destination으로 xmm 레지스터만 가능하다.
    
### 배운 점 & 느낀 점

솔직히 머리 터지겠음..

아니 이름이 왜 다 저따구야?

솔직히 이동까지는 ㅇㅋ. 이건 ㅇㅋ.
`vmovss`면 부동소수점 AVX(`v`) move(`mov`) scala(`s`) single precision(`s`).
직관적이잖아?

근데 시발 변환 명령어는 시발 저게 뭐야 진짜..
`vcvttss2si`면 
AVX(`v`) convert(`cvt`) 소수점 절삭(`t`) 
scalar single precision(`ss`) to(`2`) signed integer(`si`)..

시발 이게 맞음?
명령어 이름 더럽게 복잡하네..

이게 맞나 싶다 진짜로..

쉽게 요약하면,

- `s` : scalar
- `p` : packed
- `ss` : scalar single precision
- `sd` : scalar double precision
- `ps` : packed single precision
- `pd` : packed double precision
- `a` : aligned
- `t` : truncate
- `cvt A 2 B` : A 타입 -> B타입 변환

이렇게 보면 돼..

그리고 여기서 처음으로 나온 개념이 바로 패키지야.
여러개의 부동소수점 데이터를 묶어서 `vmov`하는 그런거라고 하는데,
이건 주소가 16바이트의 배수로 정렬이 되어있어야 한다고..

더 정확히는 하나의 `%xmm` 레지스터에서 여러 개의 부동소수점 값을 넣고
한 명령으로 같이 처리하는 것이라고 하는데..

솔직히 진짜 이거 하면서 내 대가리를 깨고싶었어..

그리고 시발 변환 명령어에서 정수 to 부동소수점 / 부동소수점 to 부동소수점은 진짜
무슨 오퍼랜드를 세 개를 씀?

미치겠음 진짜ㅋㅋ
이거 아니야.. 이거 진짜 아니야..
뒷 쪽 파트는 이거에 비해 쉽겠지?

## 2. Floating-Point Code in Procedures - 프로시저에서의 부동소수점 코드

### 정리

1. 부동소수점 인자 전달과 반환
  - `%xmm0` ~ `%xmm7` 레지스터들 통해 최대 8개의 부동소수점 인자를 전달한다.
  - 반환 값은 `%xmm0`을 사용한다.
  - 모든 XMM 레지스터는 caller-saved 방식이라
  피호출자는 해당 레지스터를 미리 저장하지 않고도 덮어쓸 수 있다.
  - 함수의 인자에 포인터 / 정수 / 부동소수점 인자들이 혼합되어있는 경우
  포인터 / 정수는 범용 레지스터를 통해,
  부동소수점 값은 XMM 레지스터를 통해 전달된다.

### 배운 점 & 느낀 점

이번 챕터는 앞의 이동 / 변환 챕터에 비해 정말 간단했어ㅜㅜ
진짜 이렇게 편한 챕터가 있을까..

그냥

- `%xmm0` ~ `%xmm7` 레지스터를 통해 인자가 전달된다.
- `%xmm0` 레지스터를 통해 반환값이 전달된다.
- 모든 XMM 레지스터는 caller-saved 레지스터이다.(호출된 프로시저가 값을 보존하지 않아도 된다.)
- 정수 / 포인터가 뒤섞여있는 경우 레지스터 종류를 따로 쓴다.

이게 끝임.. 편하다..

## 3. Floating-Point Arithmetic Operations - 부동소수점 산술 연산 명령어

### 정리

1. 부동소수점 산술 연산 명령어
    - single precision / double precision 명령어가 별개로 있다.
    - destination 오퍼랜드는 무조건 XMM 레지스터여야 한다.
    - 
    |single 명령어|double 명령어 |동작|뜻|
    |---|---|---|---|
    |`vaddss S1, S2, D`|`vaddsd`|`S2 + S1 -> D`|덧셈|
    |`vsubss S1, S2, D`|`vsubsd`|`S2 - S1 -> D`|뺄셈|
    |`vmulss S1, S2, D`|`vmulsd`|`S2 * S1 -> D`|곱셈|
    |`vdivss S1, S2, D`|`vdivsd`|`S2 / S1 -> D`|나눗셈|
    |`vmaxss S1, S2, D`|`vmaxsd`|`max(S2, S1) -> D`|최댓값|
    |`vminss S1, S2, D`|`vminsd`|`min(S2, S1) -> D`|최솟값|
    |`sqrtss S1, D`|`sqrtsd`|`sqrt(S1) -> D`|루트|

### 배운 점 & 느낀 점

이것도 `vmov` / 변환에 비하면 매우 간단해서 좋다..

그냥 단일 정밀도 / 이중 정밀도에 따라 다른 명령어를 쓰고
정수 관련 명령어들처럼 `op S, D`로 `D ? S -> D`가 아니라
오퍼랜드 세 개로 
`op S1, S2, D`로 `S2 ? S1 -> D`라는 점만 기억하면 될 듯..

어쩌면 오퍼랜드 세 개를 쓰는게 C언어를 먼저 했으면
더 익숙했을 것 같아.
(`c = b ? a`. 오퍼랜드 두 개는 `b = b ? a`라서 직관적이지 않음)
근데 나는 최근에 C언어 코딩을 안 하고 어셈블리만 주구장창 만지고있으니ㅋㅋ

## 4. Defining and Using Floating-Point Constants - 부동소수점 상수의 정의 / 사용

### 정리

1. 정수와 달리, 부동소수점 명령어들은 즉시값을 오퍼랜드로 쓸 수 없고,
모든 상수 값에 대한 저장 공간을 할당하고 초기화해줘야한다.

2. 결국 부동소수점 값도 비트열이므로,
정수로 즉시값을 따로 저장해놓고 부동소수점으로 해석하면 된다.

### 배운 점 & 느낀 점

이번도 매우 간단.

그냥 어차피 부동소수점 수도 비트열이기 때문에,
그냥 정수값으로 표현해놓고 부동소수점으로 그 비트열을 해석해버리면 끝이라는 것 같아.

그리고 그 값은 따로 저장공간을 할당받고 기록해놓으면 되고..

## 5. Using Bitwise Operations in Floating-Point Code - 부동소수점 코드에서 비트 연산 명령어 사용하기

### 정리

1. 부동소수점 비트 연산 명령어
    - 기본적으로 패키지 단위로 동작한다.
    - 
    |single 명령어|double 명령어 |동작|뜻|
    |---|---|---|---|
    |`vxorps S1, S2, D`|`vxorpd`|`S2 ^ S1 -> D`|비트 XOR 연산|
    |`vandps S1, S2, D`|`vandpd`|`S2 & S1 -> D`|비트 AND 연산|
    
2. 부동소수점 비트 연산 명령어의 쓰임새
    - 다양한 최적화를 가능하게 한다.
    - 예 :
      - `vxorpd %xmm0, %xmm0, %xmm0` = 0으로 해당 레지스터 초기화
      - `vandpd %xmm1, %xmm0, %xmm0` = `%xmm`의 값을 기반으로 비트마스킹
      - 등등..

### 배운 점 & 느낀 점

이번도 되게 간단해.

그냥 부동소수점 비트 연산 명령어 소개하고,
패키지 단위로 동작한다는거 설명하고,
대체 이걸 어따 쓰는지 설명하고 끝.

근데 저거 최적화는 되게 신기하더라..

`vxorpd %xmm0, %xmm0, %xmm0`으로 해당 레지스터를 0으로 초기화한다거나,
`vandpd %xmm1, %xmm0, %xmm0`으로 `%xmm1` 레지스터의 값에 `%xmm0`의 마스크를 씌운다거나..

어쨌든 신기했어.

## 6. Floating-Point Comparison Operations - 부동소수점 비교 명령어

### 정리

1. 부동소수점 비교 명령어
    - 이것도 single precision / double precision이 나뉘어져있다.
    - `S2 - S1`의 결과에 따라 조건 코드를 생성한다.
    - `S2`는 반드시 XMM 레지스터에 있어야 한다.
    - 그리고 이 연산은 패리티 플래그(PF)까지 설정한다.
      - ZF / CF / PF를 설정한다.
      - PF는 피연산자 중 하나가 $NaN$일 때 세팅된다.
      - 즉, 연산 실패를 판정하는데 사용된다.
    -
    |명령어|뜻|
    |---|---|
    |`vucomiss`|single precision 비교|
    |`vucomisd`|double precision 비교|
    - 
    |대소관계|CF|ZF|PF|
    |---|---|---|---|
    |불가능|`1`|`1`|`1`|
    |`S2 < S1`|`1`|`0`|`0`|
    |`S2 = S1`|`0`|`1`|`0`|
    |`S2 > S1`|`0`|`0`|`0`|

### 배운 점 & 느낀 점

이것도 솔직히 그렇게 어렵지는 않았는데,
패리티 플래그(PF)가 처음 나와서 신기했어.

여기서는 비교하는 두 값 중 하나라도 $NaN$이면 패리티 비트가 설정된다고 이해했어.
그리고 그렇게 설정된 패리티 비트로 비교할 수 없는 경우를 판단한다고 이해했고.

나머지는 unsigned 정수의 `cmp` 명령어와 같네..

## 7. 요약

1. 부동소수점은 전용 XMM 레지스터들과 별개의 명령어들을 통해 저장되고, 연산된다.
2. `%xmm0` ~ `%xmm7`을 매개변수로, `%xmm0`을 반환값으로 쓴다.
3. 모든 XMM레지스터는 caller-saved 레지스터라 피호출자가 값을 보존할 필요가 없다.
4. 부동소수점 연산에서는 scalar와 packed 연산이 있으며,
scalar는 하나의 XMM 레지스터의 한 값만 처리하고,
packed 연산은 하나의 XMM 레지스터 안의 여러 값을 동시에 처리한다.