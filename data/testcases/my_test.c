#include <cstdio>
int judgeResult = 0;
const int Mod = 253;

void printInt(int x) {
  printf("%d\n", x);
  judgeResult ^= x;
  judgeResult += 173;
  //printf("%d\n", judgeResult);
}

void printStr(const char *str) {
  for (const char *cur = str; *cur != 0; ++cur) {
    judgeResult ^= *cur;
    judgeResult += 521;
  }
}

int N;
int M = 0;
int check[20];

int main() {
  N = 10;
  int i = 0;
  while (i <= N)
    check[i++] = 1;
  int phi[15];
  int P[15];
  phi[1] = 1;
  for (i = 2;; ++i) {
    if (i > N)
      break;
    if (check[i]) {
      P[++M] = i;
      phi[i] = i - 1;
    }
    int k = i;
    int i;
    for (i = 1; i <= M && (k * P[i] <= N); i++) {
      int tmp = k * P[i];
      if (tmp > N)
        continue;
      check[tmp] = 0;
      if (k % P[i] == 0) {
        phi[tmp] = phi[k] * P[i];
        break;
      } else {
        phi[k * P[i]] = phi[k] * (P[i] - 1);
      }
    }
    printInt(phi[k]);
  }
  return judgeResult % Mod;  // 50
}

//174
//345
//520
//697
//872
//1051
//1228
//1399
//1568
