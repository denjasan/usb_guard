#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int usb_key_present(void) {
    FILE *f = fopen("/proc/usb_guard", "r");
    if (!f) return 0;
    int v = 0;
    fscanf(f, "%d", &v);
    fclose(f);
    return v == 1;
}

int main(void) {
    if (!usb_key_present()) {
        fprintf(stderr, "Нет USB-ключа: доступ запрещён.\n");
        return 1;
    }

    execl("/opt/usbkey/gnome-mines.real", "gnome-mines", (char*)NULL);
    perror("execl");
    return 1;
}
