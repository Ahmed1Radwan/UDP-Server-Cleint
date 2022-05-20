#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

  struct sockaddr_in server_addr, client_addr;
  struct stat st;
  struct packet packet;
  struct timeval t_out = {0, 0};

  char msg_recv[BUFF_SIZE];
  string command_recv = "";

  ssize_t num_read;
  ssize_t length;
  off_t f_size;

  long int ack_num = 0; // packet ack
  int ack_send = 0;
  int socket_fd;

  FILE *file_ptr;

  int packets_loss_count = 0;
  int every_loss_count = 0;
  string PLP = argv[2];
  int pointer1 = 0;
  int pointer2 = 0;
  for(int c = 0; c < PLP.length() ; c++){
    if(PLP[c] == '.'){
      pointer1 = c;
    }else if(PLP[c] != '0'){
      pointer2 = c;
      packets_loss_count = int(PLP[c] - '0');
      break;
    }
  }
  every_loss_count = int(pow(10, pointer2 - pointer1));

  //cout << "packet_loss_count " << packets_loss_count << " , every_loss_count " << every_loss_count << endl;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
    handle_errors("Error in server socket creation");
  }

  if(bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
    handle_errors("Error in server binding");
  }

  while(true){
    cout << "Waiting for client request" << endl;

    memset(msg_recv, 0, sizeof(msg_recv));

    length = sizeof(client_addr);

    if((num_read = recvfrom(socket_fd, msg_recv, BUFF_SIZE, 0, (struct sockaddr *) &client_addr, (socklen_t *) &length)) == -1){
      handle_errors("Error in server receiving");
    }
    cout << "server received from client : " << msg_recv << endl;

    string filename(msg_recv);

    cout << "filename is " << filename << endl;
    // check file exists
    if (filename != "\0" && access(&filename[0], F_OK) == 0){
      int total_packets = 0;
      int resend_packet = 0;
      int drop_packet = 0;
      int time_out_flag = 0;
      long int i = 0;

      // get the file size
      stat(&filename[0], &st);
      f_size = st.st_size;

      t_out.tv_sec = 2;
      t_out.tv_usec = 0;
      // set timeout option for recvfrom operation
      setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &t_out, sizeof(timeval));

      file_ptr = fopen(&filename[0], "rb");

      // calculate total number of packets to be sent
      if((f_size % BUFF_SIZE) != 0){
        total_packets = (f_size / BUFF_SIZE) + 1;
      }else{
        total_packets = (f_size / BUFF_SIZE);
      }

      cout << "total number of packets to send for file " << filename << " = " << total_packets << endl;

      length = sizeof(client_addr);

      // first send number off packets to the client
      sendto(socket_fd, &(total_packets), sizeof(total_packets), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
      // wait for ack form client that total number of packets received
      recvfrom(socket_fd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &client_addr, (socklen_t *)&length);

      // keep send it untll acknowledege received
      while(ack_num != total_packets){
        // first send number off packets to the client
        sendto(socket_fd, &(total_packets), sizeof(total_packets), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        // wait for ack form client that total number of packets received
        recvfrom(socket_fd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &client_addr, (socklen_t *)&length);

        resend_packet++;
        // event i time out didn't occuer and resend happens 20 times break
        if(resend_packet == 20){
          time_out_flag = 1;
          break;
        }

      }

      struct packet packets_array[total_packets + 1];
      for(i = 1; i <= total_packets; i++){
        memset(&packet, 0, sizeof(packet));
        packet.seqno = i;
        packet.length = fread(packet.data, 1, BUFF_SIZE, file_ptr);
        packets_array[i] = packet;
      }
      fclose(file_ptr);

      int ssthread = 15;
      int window_size = 1;
      int left_pointer = 1; // the packt that waiting for its ack
      int right_pointer = 1;
      long int counter = 1;
      int dup_ack = 0;

      int loss_sofar = 0;
      time_t start = time(nullptr);
      while(left_pointer <= total_packets){
        ack_num = 0;
        while(right_pointer <= total_packets && right_pointer < (left_pointer + window_size)){
          if(counter >= every_loss_count){
            counter = 1;
            loss_sofar = 0;
          }
          // check if there is packets to loss
          if(loss_sofar < packets_loss_count){
            if(counter < (every_loss_count - packets_loss_count)){
              int random = rand() % 10 + 1; // generate random number between 1 to 10
              if(random >= 5){ // iff probabilty >= 5 send the packet
                sendto(socket_fd, &packets_array[right_pointer], sizeof(packets_array[right_pointer]), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
              }else{ // don't send the packet
                loss_sofar++;
              }
            }else{
              // don't send
              loss_sofar++;
            }
          }else{
            // if all loss packets done just send the packet
            sendto(socket_fd, &packets_array[right_pointer], sizeof(packets_array[right_pointer]), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
          }
        
          counter++;
          cout << "Packet : " << right_pointer << " sent " << endl;
          right_pointer++;
        }

        recvfrom(socket_fd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *) &client_addr, (socklen_t *) &length);
        cout << "ACK : " << ack_num << " received" << endl;
        if(ack_num == total_packets){
          break;
        }
        if(ack_num == left_pointer){
          left_pointer = ack_num + 1;
          dup_ack = 0;

          if(window_size >= ssthread){
            window_size++;
          }else{
            window_size = window_size * 2;
          }
        }else {
            dup_ack++;
            if(dup_ack == 3 && ack_num != 0){
              right_pointer = ack_num + 1;
              dup_ack = 0;
              ssthread = ssthread / 2;
              window_size = 1;
            }else if(ack_num == 0){
              right_pointer = left_pointer;
            }

        }

      }
      time_t end = time(nullptr);
      cout << "***********File sent successfuly*************" << endl;
      cout << "Time Taken :- " << (end - start) << " seconds"  << endl;
      t_out.tv_sec = 0;
      t_out.tv_usec = 0;
      // disable time out option
      setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &t_out, sizeof(struct timeval));
      memset(&packets_array, 0 , sizeof(packets_array));

    }else{
      handle_errors("Error in server finding the file");
    }
  }

  close(socket_fd);
  exit(EXIT_SUCCESS);
  return 0;
}
