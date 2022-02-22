#include <fcntl.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUF_SIZE 1024

#define TAG_FILE_EXTENSION_LENGTH 0
#define TAG_FILE_EXTENSION 1
#define TAG_FILE_SIZE 2
#define TAG_FILE_CONTENT 3
#define TAG_CHARS_SENT 4

#define RANK_SENDER 0
#define RANK_RECEIVER 1

char* get_filename_ext(const char *filename) {
    char *dot = strrchr(filename, '.');
    if (!dot)
        return "";
    else
        return dot + 1;
}

int socket_connect(char *host, in_port_t port){
    struct hostent *hp;
  	struct sockaddr_in addr;
  	int on = 1, sock;

    if((hp = gethostbyname(host)) == NULL){
  		herror("gethostbyname");
  		exit(1);
    }
    bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

    if(sock == -1){
  		perror("setsockopt");
  		exit(1);
    }

    if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
  		perror("connect");
  		exit(1);
    }
    return sock;
}

int main(int argc, char *argv[]) {
    // Initialize the MPI environment
    MPI_Init(&argc, &argv);

    // Get the rank of the process
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    char buffer[BUF_SIZE];
    char* file_ext;
    int ext_length;

    if(argc < 3){
      if (rank == RANK_SENDER) {
          // Sender
          char* file_name = argv[1];

          // Send file extension
          file_ext = get_filename_ext(file_name);
          ext_length = strlen(file_ext);

          printf("Sending %s\n", file_ext);

          MPI_Send(&ext_length, 1, MPI_INT, RANK_RECEIVER, TAG_FILE_EXTENSION_LENGTH, MPI_COMM_WORLD);
          MPI_Send(file_ext, ext_length + 1, MPI_CHAR, RANK_RECEIVER, TAG_FILE_EXTENSION, MPI_COMM_WORLD);

          // Open the file to read
          int fd = open(file_name, O_RDONLY);
          if (fd < 0) exit(1);

          int size = lseek(fd, 0, SEEK_END);
          lseek(fd, 0, 0);
          MPI_Send(&size, 1, MPI_INT, RANK_RECEIVER, TAG_FILE_SIZE, MPI_COMM_WORLD);

          while (1) {
              int n = read(fd, buffer, BUF_SIZE);
              MPI_Send(&n, 1, MPI_INT, RANK_RECEIVER, TAG_CHARS_SENT, MPI_COMM_WORLD);
              if (n == 0) {
                  break;
              }

          MPI_Send(buffer, n, MPI_CHAR, RANK_RECEIVER, TAG_FILE_CONTENT, MPI_COMM_WORLD);
          }
          close(fd);

      } else if (rank == RANK_RECEIVER) {
          MPI_Recv(&ext_length, 1, MPI_INT, RANK_SENDER, TAG_FILE_EXTENSION_LENGTH, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

          char* file_ext = malloc((ext_length + 1) * sizeof(char));
          MPI_Recv(file_ext, ext_length + 1, MPI_CHAR, RANK_SENDER, TAG_FILE_EXTENSION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

          printf("Receiving %s\n", file_ext);

          // Open file to write
          const char* file_name = malloc(sizeof(char) * 20);
          strcpy(file_name, "received.");
          strcat(file_name, file_ext);
          int fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY);
          if (fd < 0) exit(1);

          int size;
          MPI_Recv(&size, 1, MPI_INT, RANK_SENDER, TAG_FILE_SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

          int chars_sent;
          while (1) {
              MPI_Recv(&chars_sent, 1, MPI_INT, RANK_SENDER, TAG_CHARS_SENT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

              if (chars_sent == 0) {
                  break;
              }
              MPI_Recv(buffer, chars_sent, MPI_CHAR, RANK_SENDER, TAG_FILE_CONTENT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              write(fd, buffer, chars_sent);
          }

          close(fd);
      }

      // Finalize the MPI environment.
      MPI_Finalize();
      return 0;
    }

    int fd = socket_connect(argv[1], atoi(argv[2]));
    write(fd, "GET /\r\n\r\n", strlen("GET /\r\n\r\n")); // write(fd, char[], len);
    bzero(buffer, BUF_SIZE);

    while(read(fd, buffer, BUF_SIZE - 1) != 0){
  		fprintf(stderr, "%s", buffer);
  		bzero(buffer, BUF_SIZE);
    }

    shutdown(fd, SHUT_RDWR);
    close(fd);


}
