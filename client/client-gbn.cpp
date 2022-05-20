#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <cstring>
#include <math.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
using namespace std;


#define BUFF_SIZE 2048

/* Each packet has sequenve number, length of the data and the actual data*/
struct packet{
  long int seqno;
  long int length;
  char data[BUFF_SIZE];
};

void handle_errors(string msg){
  cout << msg << endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv){

  if(argc < 3 || argc > 3){
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in send_addr, from_addr;
  struct stat st;
  struct packet packet;
  struct timeval t_out = {0,0};

  string filename = "";
  string ack_send = "";

  ssize_t num_read = 0;
  ssize_t length = 0;
  off_t f_size = 0;

  long int ack_num = 0;
  int socket_fd, ack_recv = 0;

  FILE *file_ptr;

  memset(&send_addr, 0, sizeof(send_addr));
  memset(&from_addr , 0, sizeof(from_addr));

  send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(atoi(argv[2]));
	send_addr.sin_addr.s_addr = inet_addr(argv[1]);

  if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
    handle_errors("Error in client socket creation");
  }

  while(true){
    filename = "";
    cout << "Enter file name to get" << endl;
    cin >> filename;

    if(filename != "\0"){
      if(sendto(socket_fd, &filename[0], filename.length(), 0, (struct sockaddr *) &send_addr, sizeof(send_addr)) == -1){
        handle_errors("Error sending filename to the server");
      }
      long int total_packets = 0;
      long int bytes_recv = 0;
      long int i = 0;

      t_out.tv_sec = 2;
      setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &t_out, sizeof(struct timeval));

      recvfrom(socket_fd, &total_packets, sizeof(total_packets),  0, (struct sockaddr *) &from_addr, (socklen_t *) &length);


      if(total_packets > 0){
        sendto(socket_fd, &total_packets, sizeof(&total_packets), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
        cout << "number of packets that will be received : " << total_packets << endl;

        file_ptr = fopen(&filename[0],"wb");

        int left_pointer = 1; // packe that waiting for
        int right_pointer = 1;
        //int window_size = 5;

        while(true){

          memset(&packet, 0, sizeof(packet));

          recvfrom(socket_fd, &(packet), sizeof(packet), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length);
          cout << "Packet : " << packet.seqno << " received," << " left_pointer is " << left_pointer << endl;
          if(packet.seqno == left_pointer){
            fwrite(packet.data, 1, packet.length, file_ptr);
            sendto(socket_fd, &(packet.seqno), sizeof(packet.seqno), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
            left_pointer++;
            bytes_recv += packet.length;
          }else{
            int x = left_pointer - 1;
            sendto(socket_fd, &(x), sizeof(x), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
          }
          if(left_pointer == total_packets +1){
            cout << "****************file received successfuly**************" << endl;
            break;
          }
        }
        while(int read = recvfrom(socket_fd, &(packet), sizeof(packet), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length) != -1){

        }


        cout << "bytes received is : " << bytes_recv << endl;
        fclose(file_ptr);
      }else{
        cout << "file is empty" << endl;
      }

    }


  }
  close(socket_fd);
  exit(EXIT_SUCCESS);
  return 0;
}
