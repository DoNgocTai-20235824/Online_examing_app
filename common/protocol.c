#include "protocol.h"
#include <arpa/inet.h>

void header_to_network(Header* hdr){
    if(!hdr) return;
    hdr->payload_len = htonl(hdr->payload_len);
    hdr->opcode = htons(hdr->opcode);
}

void header_to_host(Header* hdr){
    if(!hdr) return;
    hdr->payload_len = ntohl(hdr->payload_len);
    hdr->opcode = ntohs(hdr->opcode);
}