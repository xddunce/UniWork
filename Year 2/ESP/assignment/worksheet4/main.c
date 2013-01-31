
#include <bios/stdio.h>
extern void putstring(int uart, char *string);
extern void putnws(char *string, int uart);
void stripwhitespace(char *string);

int main() {
  //  printf("confirmation\r\n");
  putstring(1, "test string\r\n");
  putnws("n o w h i t e s p a c e ! ! ! ! !", 1);
  for(;;);
  return 0;
}

void stripwhitespace(char *string) {
  int i, writeIndex = 0;
  for (i = 0; string[i] != '\0'; i++) {
    if (string[i] != ' ') {
      string[writeIndex] = string[i];
      writeIndex++;
    }
    else {
      //      string[writeIndex] = string[i];
    }
  }
  string[writeIndex] = '\0';
}
