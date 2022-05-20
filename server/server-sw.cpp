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
  long int seqno; // sequence number
  long int length; // length of data can be less than Buffer size
  char data[BUFF_SIZE]; // actual data chunck
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

      long int counter = 1;
      int loss_sofar = 0;
      time_t start = time(nullptr);
      for(i = 1; i <= total_packets; i++){

        if(counter >= every_loss_count){
          counter = 1;
          loss_sofar = 0;
        }

        memset(&packet, 0, sizeof(packet));
        ack_num = 0;
        packet.seqno = i;
        packet.length = fread(packet.data, 1, BUFF_SIZE, file_ptr);
        // check if there is packets to loss
        if(loss_sofar < packets_loss_count){
          if(counter < (every_loss_count - packets_loss_count)){
            int random = rand() % 10 + 1; // generate random number between 1 to 10
            if(random >= 5){ // iff probabilty >= 5 send the packet
              sendto(socket_fd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
            }else{ // don't send the packet
              loss_sofar++;
            }
          }else{
            // don't send
            loss_sofar++;
          }
        }else{
          // if all loss packets done just send the packet
          sendto(socket_fd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        }
        counter++;
        
        recvfrom(socket_fd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *) &client_addr, (socklen_t *) &length);
        //cout << "ACk received: " << ack_num << endl;
        while(ack_num != packet.seqno){
          //sendto(socket_fd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
          if(loss_sofar < packets_loss_count){
            if(counter < (every_loss_count - packets_loss_count)){
              int random = rand() % 10 + 1; // generate random number between 1 to 10
              if(random >= 5){ // iff probabilty >= 5 send the packet
                sendto(socket_fd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
              }else{ // don't send the packet
                loss_sofar++;
              }
            }else{
              // don't send
              loss_sofar++;
            }
          }else{
            // if all loss packets done just send the packet
            sendto(socket_fd, &packet, sizeof(packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
          }
          recvfrom(socket_fd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *) &client_addr, (socklen_t *) &length);
          drop_packet++;
          resend_packet++;
          counter++;
          cout << "packet : " << packet.seqno << " -> dropped " << drop_packet << " times" << endl;

          if(resend_packet == 200){
            time_out_flag = 1;
            break;
          }
        }
        resend_packet = 0;
        drop_packet = 0;

        if(time_out_flag == 1){
          handle_errors("File didn't transfered because of too much resend and packet dropped");
          break;
        }
        cout << "packet : " << i << " ,ack : " << ack_num << endl;
        if(total_packets == ack_num){
          cout << "***********File sent successfuly*************" << endl;
        }
      }
      time_t end = time(nullptr);
      cout << "Time Taken :- " << (end - start) << " seconds"  << endl;
      fclose(file_ptr);
      t_out.tv_sec = 0;
      t_out.tv_usec = 0;
      // disable time out option
      setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &t_out, sizeof(struct timeval));
    }else{
      handle_errors("Error in server finding the file");
    }
  }

  close(socket_fd);
  exit(EXIT_SUCCESS);
  return 0;
}
