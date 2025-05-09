#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

enum {
	MAX_LINEA = 1024,
	MAX_ARGS = 64,
	MAX_PATH = 1024,
};

//Funcion para borrar el ultimo espacio ya que cuando hacía una redireccion con $ me detectaba el espacio
void
borrarUltimoEspacio(char *nombre)
{

	if (nombre == NULL) {
		return;
	}

	char *posicion = strrchr(nombre, ' ');

	if (posicion != NULL) {
		*posicion = '\0';
	}
}

void
ejecutarCd(char *path)
{
	if (path == NULL || strcmp(path, "") == 0) {
		path = getenv("HOME");
		if (path == NULL) {
			fprintf(stderr,
				"No se pudo determinar el directorio HOME\n");
			return;
		}
	}

	if (chdir(path) != 0) {
		perror("Error al cambiar de directorio");
	}
}

char *
buscarPath(char *comando)
{
	char *path_env = getenv("PATH");

	//printf("%s\n",path_env); -> Para entender las rutas
	char *token;
	char *paths;
	char *resultado;
	char ruta[MAX_PATH];

	if (!path_env)
		return NULL;

	paths = malloc(strlen(path_env) + 1);
	if (!paths)
		return NULL;

	strcpy(paths, path_env);
	token = strtok(paths, ":");
	while (token != NULL) {
		strcpy(ruta, token);
		strcat(ruta, "/");
		strcat(ruta, comando);

		if (access(ruta, X_OK) == 0) {
			resultado = malloc(strlen(ruta) + 1);
			if (!resultado) {
				free(paths);
				return NULL;
			}
			strcpy(resultado, ruta);
			free(paths);
			return resultado;
		}

		token = strtok(NULL, ":");
	}

	free(paths);
	return NULL;
}

char *
expandirVariable(char *token)
{
	char *nombre;
	char *valor;

	if (token[0] != '$') {
		char *copia = malloc(strlen(token) + 1);

		if (!copia) {
			fprintf(stderr, "error: no se pudo asignar memoria\n");
			return NULL;
		}
		strcpy(copia, token);
		return copia;
	}

	nombre = token + 1;
	valor = getenv(nombre);

	if (!valor) {
		fprintf(stderr, "error: var %s does not exist\n", nombre);
		return NULL;
	}

	char *resultado = malloc(strlen(valor) + 1);

	if (!resultado) {
		fprintf(stderr, "error: no se pudo asignar memoria\n");
		return NULL;
	}

	strcpy(resultado, valor);
	return resultado;
}

void
ejecutarComando(char **argv, char *salidaRedirigida, char *entradaRedirigida,
		int background)
{
	int pid = fork();
	char rutaLocal[MAX_PATH];
	char *ruta;
	int fd_out, fd_in;
	int status;

	borrarUltimoEspacio(salidaRedirigida);

	switch (pid) {
	case -1:
		fprintf(stderr, "Error en el fork\n");
		return;

	case 0:
		// Si hay que redirigir salida
		if (salidaRedirigida != NULL) {
			fd_out =
			    open(salidaRedirigida, O_WRONLY | O_CREAT | O_TRUNC,
				 0644);
			if (fd_out < 0) {
				perror("Error al abrir archivo de salida");
				exit(EXIT_FAILURE);
			}
			dup2(fd_out, STDOUT_FILENO);
			close(fd_out);
		}
		// Redirección de entrada
		if (entradaRedirigida != NULL) {
			fd_in = open(entradaRedirigida, O_RDONLY);
			if (fd_in < 0) {
				perror("Error al abrir archivo de entrada");
				exit(EXIT_FAILURE);
			}
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
		} else if (background) {
			// Si está en segundo plano y no tiene entrada redirigida
			fd_in = open("/dev/null", O_RDONLY);
			if (fd_in < 0) {
				perror("Error al abrir /dev/null");
				exit(EXIT_FAILURE);
			}
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
		}
		// Ejecuta desde ruta local
		strcpy(rutaLocal, "./");
		strcat(rutaLocal, argv[0]);
		if (access(rutaLocal, X_OK) == 0) {
			execv(rutaLocal, argv);
			perror("Error al ejecutar desde ruta local");
			exit(EXIT_FAILURE);
		}
		// Ejecuta desde PATH
		ruta = buscarPath(argv[0]);
		if (ruta != NULL) {
			execv(ruta, argv);
		}

		fprintf(stderr, "Comando no encontrado: %s\n", argv[0]);
		exit(EXIT_FAILURE);

	default:
		// Espera si no está en segundo plano
		if (!background) {
			waitpid(pid, &status, 0);
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				fprintf(stderr,
					"El comando terminó con error\n");
			}
		}
		break;
	}
}

int
main()
{
	char linea[MAX_LINEA];
	char *argv[MAX_ARGS];
	char *token, *saveptr;
	char *salida;
	char *entrada;
	char *nombre;
	char *valor;
	char *exp;
	char *igual;
	char *parteSalida;

	int argc = 0;
	int len;
	int background = 0;

	printf("vlite> ");
	while (fgets(linea, sizeof(linea), stdin) != NULL) {
		len = strlen(linea);
		if (len > 0 && linea[len - 1] == '\n')
			linea[len - 1] = '\0';

		salida = NULL;
		entrada = NULL;
		background = 0;
		argc = 0;

		// Busca el & y si no lo encuentra no entra
		len = strlen(linea);
		if (len > 0 && linea[len - 1] == '&') {
			background = 1;
			linea[len - 1] = '\0';
			while (len > 0
			       && (linea[len - 1] == ' '
				   || linea[len - 1] == '\t'))
				linea[--len] = '\0';
		}

		parteSalida = strtok(linea, ">");
		if ((salida = strtok(NULL, "")) != NULL) {
			while (*salida == ' ' || *salida == '\t')
				salida++;
		}
		// Maneja '<' redirección de entrada
		char *parteEntrada = strtok(parteSalida, "<");

		if ((entrada = strtok(NULL, "")) != NULL) {
			while (*entrada == ' ' || *entrada == '\t')
				entrada++;
		}
		// Tokeniza argumentos
		token = strtok_r(parteEntrada, " \t", &saveptr);
		while (token != NULL && argc < MAX_ARGS - 1) {
			igual = strchr(token, '=');

			if (igual != NULL && igual != token
			    && strchr(token, '$') == NULL) {
				*igual = '\0';
				nombre = token;
				valor = igual + 1;

				if (setenv(nombre, valor, 1) != 0) {
					perror
					    ("error al asignar variable de entorno");
				}

				token = strtok_r(NULL, " \t", &saveptr);
				continue;
			}

			exp = expandirVariable(token);
			if (exp == NULL) {
				argc = 0;
				break;
			}
			argv[argc++] = exp;

			token = strtok_r(NULL, " \t", &saveptr);
		}

		argv[argc] = NULL;

		if (argv[0] == NULL) {
			printf("vlite> ");
			continue;
		}

		if (strcmp(argv[0], "cd") == 0) {
			if (argc == 1)
				ejecutarCd(NULL);
			else if (argc == 2)
				ejecutarCd(argv[1]);
			else
				fprintf(stderr, "cd: demasiados argumentos\n");
		} else {
			ejecutarComando(argv, salida, entrada, background);
		}

		// Libera memoria de argv
		for (int i = 0; i < argc; i++) {
			free(argv[i]);
		}

		printf("vlite> ");
	}

	printf("\n");
	return 0;
}
