# Emb. Sys. prj

## 주의사항

- 컴파일에 rt 라이브러리가 필요함

- gcc -o 이름 경로 *-lrt*

## 구성요소

driver_\*.c : 드라이버 /dev/my_\*_driver

main.c : 메인 코드

dbg.c : 작업 영역(메인코드 복사본)

- FIXME : 얼굴방향인식관련

- TODO : 모터관련

$$
X[k] = \sum^{N-1}_{n=0}x[n]W_N^{nk},k=0,\cdots,N-1\\
W_N^{nk}= \exp\left(-j\frac{2\pi}{N}nk\right)
$$
