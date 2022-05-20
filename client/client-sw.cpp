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

      t_out.tv_sec = 0;
      setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 	//Disable the timeout option

      if(total_packets > 0){
        sendto(socket_fd, &total_packets, sizeof(&total_packets), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
        cout << "number of packets that will be received : " << total_packets << endl;

        file_ptr = fopen(&filename[0],"wb");

        for(int  i = 1; i <= total_packets; i++){
          memset(&packet, 0, sizeof(packet));

          recvfrom(socket_fd, &(packet), sizeof(packet), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length);
					sendto(socket_fd, &(packet.seqno), sizeof(packet.seqno), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));

          if(packet.seqno < i || packet.seqno > i){
            i--;
          }else{
            fwrite(packet.data, 1, packet.length, file_ptr);
            cout << "packet with seqno : " << packet.seqno << " received successfuly"<<endl;
            bytes_recv += packet.length;
          }
          if(i == total_packets){
            cout << "************file received successfuly************"<< endl;
          }
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
