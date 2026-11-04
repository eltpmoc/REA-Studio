// SPDX-License-Identifier: GPL-3.0-or-later
// Native ELF fixture. Addresses are discovered from symbols, never fixed by the application.
#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif
NOINLINE int somar(int a,int b){return a+b;}
NOINLINE int fatorial(int n){if(n<0 || n>12)return -1;int value=1;for(int i=2;i<=n;++i)value*=i;return value;}
NOINLINE bool eh_primo(int n){if(n<2)return false;for(int d=2;d<=n/d;++d)if(n%d==0)return false;return true;}
NOINLINE int classificar(int n){switch(n%4){case 0:return 10;case 1:return 20;case 2:return 30;default:return 40;}}
int main(){return somar(fatorial(4),classificar(eh_primo(7)?1:2))==44?0:1;}
