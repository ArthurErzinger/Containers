#include <stdio.h>
#include <unistd.h> // For fork(), getpid()
#include <sys/types.h> // For pid_t
#include <stdlib.h> // For exit()

int main() {
    pid_t pid = getpid();
    printf("Processo pai ocupado iniciado com PID: %d\n", pid);
    printf("Use este PID no seu monitor de recursos para ver o uso de 2 núcleos.\n");
    fflush(stdout);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("Erro ao criar processo filho");
        return 1;
    } else if (child_pid == 0) {
        // Child process
        printf("Processo filho ocupado iniciado com PID: %d\n", getpid());
        fflush(stdout);
        while (1) {
            // Busy loop for child
        }
    } else {
        // Parent process
        while (1) {
            // Busy loop for parent
        }
    }

    return 0; // Should not be reached
}
