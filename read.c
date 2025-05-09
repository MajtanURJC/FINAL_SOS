#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

enum{
    BUFFER_SIZE = 100
};

int main(int argc , char *argv[]) {
    char buffer[BUFFER_SIZE];
    int bytesLeidos;
    int bytesWriten;
    int archivo; 
    int fd[2];
    if (pipe(fd) < 0) {
        fprintf(stderr,"Error al crear el pipe");
        exit(1);
    }

    argc--;
    argv++;

    printf("Escribe algo (Ctrl+D para terminar):\n");

    // Leer desde entrada estándar
    bytesLeidos = read(STDIN_FILENO, buffer, BUFFER_SIZE);
    if (bytesLeidos < 0) {
        perror("Error al leer");
        exit(EXIT_FAILURE);
    }

    bytesWriten = write(fd[1],buffer,bytesLeidos);
    if(bytesWriten != bytesLeidos){
        fprintf(stderr,"Error al escribir\n");
        exit(1);
    }
    close(fd[1]);

    

    bytesLeidos = read(fd[0], buffer, BUFFER_SIZE);
    if (bytesLeidos < 0) {
        perror("Error al leer");
        exit(EXIT_FAILURE);
    }
    close(fd[0]);

    archivo = open(argv[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(archivo < 0){
        fprintf(stderr,"No se ha abierto bien el fichero\n");
        exit(1);
    }

    bytesWriten = write(archivo,buffer,bytesLeidos);
    if(bytesWriten != bytesLeidos){
        fprintf(stderr,"Error al escribir\n");
        exit(1);
    }

    close(archivo);
    return 0;
}
