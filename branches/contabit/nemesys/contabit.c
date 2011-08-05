//contabit per python by DMNK1220
//
//Linux: gcc contabit.c -shared -I/usr/include/python2.7 -lpython2.7 -lpcap -ocontabit.so
//
//Win: gcc contabit.c -shared -I C:\Python27\include -I C:\winpcap\Include -I C:\winpcap\Include\pcap -L C:\Programmi\CodeBlocks\MinGW\lib -L C:\Python27\libs -L C:\winpcap\Lib -o contabit.pyd -lpython27 -lwpcap -lwsock32
//
// gcc -fPIC                        [per 64bit .so .pyd]
// gcc -fno-stack-protector         [disabilitare la protezione di overflow]

#include <contabit.h>

int DEBUG_MODE=0;
FILE *debug_log;

int err_flag=0;
char err_str[88]="No Error";

int ind_dev=0, num_dev=0, buffer=0;

u_int pkt_tot_start=0;
u_long myip_int=0, ip_nem_int=0;

pcap_t *handle;

struct devices device[22];
struct pcap_stat pcapstat;
struct statistics mystat;


int invalid_ip (const char *ip_string)
{
	char ip_cmp[88];
	u_int ip1,ip2,ip3,ip4;

	if(ip_string==NULL)
	{return 1;}

	if(sscanf(ip_string,"%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) != 4)
	{return 1;}

	if((ip1 != 0) && (ip1 <= 255) && (ip2 <= 255) && (ip3 <= 255) && (ip4 <= 255))
	{
		sprintf(ip_cmp,"%u.%u.%u.%u",ip1,ip2,ip3,ip4);
		if(strcmp(ip_cmp,ip_string)!=0){return 1;}
		return 0;
	}
	return 1;
}


void mycallback (u_char *user, const struct pcap_pkthdr *hdr, const u_char *data)
{
    const struct hdr_ethernet *ethernet;    /* The ethernet header */
    const struct hdr_arp *arp;              /* The Arp header */
	const struct hdr_ipv4 *ipv4;            /* The IPv4 header */
	const struct hdr_ipv6 *ipv6;            /* The IPv6 header */
	const struct hdr_tcp *tcp;              /* The TCP header */
	const char *payload;                    /* Packet payload */

	struct in_addr addr;

	u_int size_ipv4=0, size_ipv6=40, size_tcp=0, size_payload=0;

    char type[22], buff_temp[22], mac_src[22], mac_dst[22], src[22], dst[22], up_down[22], txrx[3];
    int proto=0, hdr_len=0, pktpad=0;

    ethernet = (struct hdr_ethernet*)(data);

//    Estrarre indirizzi MAC
//    sprintf(mac_src,"%i",ether_ntoa((struct ether_addr*)ethernet->ether_shost));
//    sprintf(mac_dst,"%i",ether_ntoa((struct ether_addr*)ethernet->ether_dhost));

    proto = ntohs(ethernet->ether_type);

    switch(proto)
    {
    case 0x0800 :   strcpy(type,"IPv4");

                    ipv4 = (struct hdr_ipv4*)(data + SIZE_ETHERNET);
                    size_ipv4 = IP_HL(ipv4)*4;

                    strcpy(src,inet_ntoa(ipv4->ip_src));
                    strcpy(dst,inet_ntoa(ipv4->ip_dst));

                    switch ((int)(ipv4->ip_p))
                    {
                    case 6  :   strcat(type,"|TCP");
                                tcp = (struct hdr_tcp*)(data + SIZE_ETHERNET + size_ipv4);
                                size_tcp = TH_OFF(tcp)*4;

                                hdr_len=(SIZE_ETHERNET + size_ipv4 + size_tcp);
                                payload = (u_char *)(data + SIZE_ETHERNET + size_ipv4 + size_tcp);
                                size_payload = ntohs(ipv4->ip_len) - (size_ipv4 + size_tcp);
                                break;

                    case 17 :   strcat(type,"|UDP");
                                hdr_len=(SIZE_ETHERNET + size_ipv4 + 8);
                                payload = (u_char *)(data + SIZE_ETHERNET + size_ipv4 + 8);
                                size_payload = ntohs(ipv4->ip_len) - (size_ipv4 + 8);
                                break;

                    case 1  :   strcat(type,"|ICMPv4");
                                break;

                    case 2  :   strcat(type,"|IGMPv4");
                                break;

                    case 89 :   strcat(type,"|OSPF");
                                break;

                    default :   sprintf(buff_temp,"|T:%d",(int)(ipv4->ip_p));
                                strcat(type,buff_temp);
                                break;
                    }
                    break;

    case 0x86dd :   strcpy(type,"IPv6");

                    ipv6 = (struct hdr_ipv6*)(data + SIZE_ETHERNET);
                    size_ipv6 = 40;

                    sprintf(src,"***.***.***.***");
                    sprintf(dst,"***.***.***.***");

                    switch (ipv6->next_header)
                    {
                    case 6  :   strcat(type,"|TCP");
                                tcp = (struct hdr_tcp*)(data + SIZE_ETHERNET + size_ipv6);
                                size_tcp = TH_OFF(tcp)*4;

                                hdr_len=(SIZE_ETHERNET + size_ipv6 + size_tcp);
                                payload = (u_char *)(data + SIZE_ETHERNET + size_ipv6 + size_tcp);
                                size_payload = ntohs(ipv6->payload_len) - (size_tcp);
                                break;
                    case 17 :   strcat(type,"|UDP");
                                hdr_len=(SIZE_ETHERNET + size_ipv6 + 8);
                                payload = (u_char *)(data + SIZE_ETHERNET + size_ipv6 + 8);
                                size_payload = ntohs(ipv6->payload_len) - (8);
                                break;
                    case 1  :   strcat(type,"|ICMPv4");
                                break;
                    case 2  :   strcat(type,"|IGMPv4");
                                break;
                    case 58 :   strcat(type,"|ICMPv6");
                                break;
                    default :   sprintf(buff_temp,"|T:%d",ipv6->next_header);
                                strcat(type,buff_temp);
                                break;
                    }

                    break;

    case 0x0806 :   strcpy(type,"ARP\t");
                    arp = (struct hdr_arp*)(data + SIZE_ETHERNET);
                    strcpy(src,inet_ntoa (*(struct in_addr*)(arp->arp_src)));
                    strcpy(dst,inet_ntoa (*(struct in_addr*)(arp->arp_dst)));
                    break;

    default     :   strcpy(type,"UNKNOW:");
                    if(proto<0x05dd)
                    {
                        sprintf(buff_temp,"%i byte",proto);
                    }
                    else
                    {
                        sprintf(buff_temp,"%x",proto);
                    }
                    strcat(type,buff_temp);
                    sprintf(src,"***.***.***.***");
                    sprintf(dst,"***.***.***.***");
                    break;
    }

    if((inet_addr(src)==myip_int || inet_addr(src)==ip_nem_int) && (inet_addr(dst)==myip_int || inet_addr(dst)==ip_nem_int))
    {
        strcat(type,"|NEM");
    }


    if(myip_int!=inet_addr(src))
    {
        strcpy(txrx,"Rx");

        sprintf(up_down,"0\t%d",((hdr->len)+4));

        mystat.pkt_down_all++;
        mystat.pkt_tot_all++;

        mystat.byte_down_all += ((hdr->len)+4);
        mystat.byte_tot_all += ((hdr->len)+4);

        mystat.payload_down_all += size_payload;
        mystat.payload_tot_all += size_payload;

        if((inet_addr(src)==ip_nem_int) && (inet_addr(dst)==myip_int))
        {
            mystat.pkt_down_nem++;
            mystat.pkt_tot_nem++;

            mystat.byte_down_nem += ((hdr->len)+4);
            mystat.byte_tot_nem += ((hdr->len)+4);

            mystat.payload_down_nem += size_payload;
            mystat.payload_tot_nem += size_payload;
        }
        else
        {
            mystat.pkt_down_oth++;
            mystat.pkt_tot_oth++;

            mystat.byte_down_oth += ((hdr->len)+4);
            mystat.byte_tot_oth += ((hdr->len)+4);

            mystat.payload_down_oth += size_payload;
            mystat.payload_tot_oth += size_payload;
        }

    }
    else
    {
        strcpy(txrx,"Tx");

        if((pktpad=(60-(hdr->len)))>0)
        {
            sprintf(up_down,"%d+%d\t0",((hdr->len)+4),pktpad);
        }
        else
        {
            sprintf(up_down,"%d\t0",((hdr->len)+4));
        }

        mystat.pkt_up_all++;
        mystat.pkt_tot_all++;

        mystat.byte_up_all += ((hdr->len)+pktpad+4);
        mystat.byte_tot_all += ((hdr->len)+pktpad+4);

        mystat.payload_up_all += size_payload;
        mystat.payload_tot_all += size_payload;

        if((inet_addr(src)==myip_int) && (inet_addr(dst)==ip_nem_int))
        {
            mystat.pkt_up_nem++;
            mystat.pkt_tot_nem++;

            mystat.byte_up_nem += ((hdr->len)+pktpad+4);
            mystat.byte_tot_nem += ((hdr->len)+pktpad+4);

            mystat.payload_up_nem += size_payload;
            mystat.payload_tot_nem += size_payload;
        }
        else
        {
            mystat.pkt_up_oth++;
            mystat.pkt_tot_oth++;

            mystat.byte_up_oth += ((hdr->len)+pktpad+4);
            mystat.byte_tot_oth += ((hdr->len)+pktpad+4);

            mystat.payload_up_oth += size_payload;
            mystat.payload_tot_oth += size_payload;
        }
    }

    pcap_stats(handle,&pcapstat);

    mystat.pkt_pcap_tot=pcapstat.ps_recv-pkt_tot_start;
    mystat.pkt_pcap_drop=pcapstat.ps_drop;
    mystat.pkt_pcap_dropif=pcapstat.ps_ifdrop;

    if(DEBUG_MODE)
    {
        fprintf(debug_log,"%li)\t%s\t%s\t%s\t\t%s\t%li,%.6li\t%s\t",mystat.pkt_pcap_tot,src,dst,up_down,type,hdr->ts.tv_sec,hdr->ts.tv_usec,txrx);
        fprintf(debug_log,"SRC: %s DST: %s LEN: %i=%i+%i+4(CRC)\n",mac_src,mac_dst,(hdr->len)+4, hdr_len, size_payload);
        fprintf(debug_log,"\n");
    }
}


void find_devices()
{
    int i=0, test=0;

    char *ip, *net, *mask, *point;
    char errbuf[PCAP_ERRBUF_SIZE];

    struct in_addr addr;

    bpf_u_int32 netp, maskp;

    pcap_if_t *alldevs, *dl;

    #if _WIN32
    WSADATA wsa_Data;
    char HostName[255];
    struct hostent *host_entry;
    #endif

    ind_dev=0;

    if (pcap_findalldevs (&alldevs, errbuf) != 0)
    {sprintf(err_str,"FindAllDevs error: %s\n",errbuf);err_flag=-1;}

    dl=alldevs;

    for(dl=alldevs; dl; dl=dl->next)
    {
        ind_dev++;

        device[ind_dev].name=dl->name;

        //printf("\nNAME: %s",device[ind_dev].name);

        if (pcap_lookupnet(dl->name, &netp, &maskp, errbuf) != 0)
        {sprintf (err_str,"LookUpNet error: %s", errbuf);err_flag=-1;}

        addr.s_addr = netp;
        net = inet_ntoa(addr);
        device[ind_dev].net=(char*)calloc(22,sizeof(char));
        memcpy(device[ind_dev].net,net,strlen(net)+1);

        //printf("\nNET: %s",device[ind_dev].net);

        addr.s_addr = maskp;
        mask = inet_ntoa(addr);
        device[ind_dev].mask=(char*)calloc(22,sizeof(char));
        memcpy(device[ind_dev].mask,mask,strlen(mask)+1);

        //printf("\nMASK: %s",device[ind_dev].mask);

        if(dl->addresses!=NULL)
        {
            addr.s_addr = ((struct sockaddr_in *)(dl->addresses->addr))->sin_addr.s_addr;
            ip = inet_ntoa(addr);

            point=strrchr(device[ind_dev].net,'.');
            i=point-device[ind_dev].net+1;

            if(strncmp(ip,device[ind_dev].net,i) != 0)
            {
                #if _WIN32
                WSAStartup(0x101,&wsa_Data);
                gethostname(HostName, 255);
                host_entry = gethostbyname(HostName);
                ip = inet_ntoa (*(struct in_addr *)*host_entry->h_addr_list);
                WSACleanup();
                #else
                while((strncmp(ip,device[ind_dev].net,i) != 0) && (dl->addresses->next))
                {
                    dl->addresses=dl->addresses->next;
                    addr.s_addr = ((struct sockaddr_in *)(dl->addresses->addr))->sin_addr.s_addr;
                    ip = inet_ntoa(addr);
                }
                #endif
            }

            device[ind_dev].ip=(char*)calloc(22,sizeof(char));
            memcpy(device[ind_dev].ip,ip,strlen(ip)+1);

        }
        else
        {
            device[ind_dev].ip="0.0.0.0";
        }

        //printf("\nIP: %s\n\n",device[ind_dev].ip);

    }
}


void stoploop ()
{
    pcap_breakloop(handle);
}


void startloop()
{
    int pkt_diff=0;

    char errbuf[PCAP_ERRBUF_SIZE];

    memset(&mystat,0,sizeof(struct statistics));

    if (buffer<32*1024000) {buffer=32*1024000;}

    if ((handle=pcap_create(device[num_dev].name,errbuf)) == NULL)
    {sprintf (err_str,"Couldn't open device: %s",errbuf);err_flag=-1;}

    if (pcap_set_promisc(handle,1) != 0)
    {sprintf(err_str,"PromiscuousMode error: %s",errbuf);err_flag=-1;}

    if (pcap_set_timeout(handle,1) != 0)
    {sprintf(err_str,"Timeout error: %s",errbuf);err_flag=-1;}

    if (pcap_set_snaplen(handle,BUFSIZ) != 0)
    {sprintf(err_str,"Snapshot error: %s",errbuf);err_flag=-1;}

    if (pcap_set_buffer_size(handle,buffer) !=0)
    {sprintf(err_str,"SetBuffer error: %s",errbuf);err_flag=-1;}

    if (pcap_activate(handle) !=0)
    {sprintf(err_str,"Activate error: %s",errbuf);err_flag=-1;}

    pcap_stats(handle,&pcapstat);

    if((pcapstat.ps_recv)>0 || (pcapstat.ps_drop)>0 || (pcapstat.ps_ifdrop)>0)
    {
        pkt_tot_start=pcapstat.ps_recv;
        pcapstat.ps_drop=0;
        pcapstat.ps_ifdrop=0;
    }

    Py_BEGIN_ALLOW_THREADS;

    pcap_loop(handle, -1, mycallback, NULL);

    pcap_stats(handle,&pcapstat);

    mystat.pkt_pcap_tot=pcapstat.ps_recv-pkt_tot_start;
    mystat.pkt_pcap_drop=pcapstat.ps_drop;
    mystat.pkt_pcap_dropif=pcapstat.ps_ifdrop;

    if((pkt_diff=pcapstat.ps_recv-pkt_tot_start-pcapstat.ps_drop-mystat.pkt_tot_all)>0)
    {
        pcap_loop(handle, pkt_diff, mycallback, NULL);
    }

    Py_END_ALLOW_THREADS;

    pcap_close(handle);
}




static PyObject *contabit_initialize(PyObject *self, PyObject *args)
{
    int i=0;

    char *dev_sel=(char*)calloc(88,sizeof(char)), *nem=(char*)calloc(22,sizeof(char));

    err_flag=0; strcpy(err_str,"No Error");

    if(DEBUG_MODE) {debug_log = fopen("contabit.txt","w");}     //DEBUG

    PyArg_ParseTuple(args, "sz|i", &dev_sel, &nem, &buffer);

    find_devices();

    for(i=1; i<=ind_dev; i++)
    {
        if ((strcmp(dev_sel,device[i].name)==0)||(strcmp(dev_sel,device[i].ip)==0))
        {
            num_dev=i;
        }
    }

    if (num_dev==0)
    {sprintf(err_str,"Device Not Found");err_flag=-1;}

    if (invalid_ip(nem)) {strcpy(nem,"none");}

    myip_int=inet_addr(device[num_dev].ip);
    ip_nem_int=inet_addr(nem);

    return Py_BuildValue("i",err_flag);
}

static PyObject *contabit_start(PyObject *self)
{
    err_flag=0; strcpy(err_str,"No Error");

    startloop();

    return Py_BuildValue("i",err_flag);
}

static PyObject *contabit_stop(PyObject *self)
{
    err_flag=0; strcpy(err_str,"No Error");

    stoploop();

    if(DEBUG_MODE) {fclose(debug_log);}     //DEBUG

    return Py_BuildValue("i",err_flag);
}

static PyObject *contabit_getdev(PyObject *self, PyObject *args)
{
    int i=0, in_net=0, find_dev=0;

    char build_string[222];

    char *dev=(char*)calloc(88,sizeof(char));

    err_flag=0; strcpy(err_str,"No Error");

    PyArg_ParseTuple(args, "|z",&dev);

    find_devices();

    if (dev!=NULL)
    {
        for(i=1; i<=ind_dev; i++)
        {
            if ((strcmp(dev,device[i].name)==0)||(strcmp(dev,device[i].ip)==0))
            {
                in_net++; //servirà per sapere quanti ip della stessa sottorete (quanti dev sulla macchina connessi al router adsl)
                find_dev=i;
            }
        }

        if(find_dev!=0)
        {
            if (strcmp(dev,device[find_dev].name)==0)
            {
                return Py_BuildValue ("{s:i,s:s,s:s,s:s,s:s}",
                                      "err_flag",err_flag,"err_str",err_str,
                                      "dev_ip",device[find_dev].ip,"dev_net",device[find_dev].net,"dev_mask",device[find_dev].mask);
            }
            else if (strcmp(dev,device[find_dev].ip)==0)
            {
                return Py_BuildValue ("{s:i,s:s,s:s,s:s,s:s}",
                                      "err_flag",err_flag,"err_str",err_str,
                                      "dev_name",device[find_dev].name,"dev_net",device[find_dev].net,"dev_mask",device[find_dev].mask);
            }
        }
        else
        {
            sprintf(err_str,"Device Not Found");err_flag=-1;
            return Py_BuildValue ("{s:i,s:s}",
                                  "err_flag",err_flag,"err_str",err_str);
        }
    }

    strcpy(build_string,"{s:i,s:s,s:i");

    for(i=1; i<=ind_dev; i++)
    {
        strcat(build_string,",s:s,s:s");
    }

    strcat(build_string,"}");

    return Py_BuildValue (build_string,
                          "err_flag",err_flag,"err_str",err_str,"num_dev",ind_dev,
                          "dev1_name",device[1].name,"dev1_ip",device[1].ip,"dev2_name",device[2].name,"dev2_ip",device[2].ip,
                          "dev3_name",device[3].name,"dev3_ip",device[3].ip,"dev4_name",device[4].name,"dev4_ip",device[4].ip,
                          "dev5_name",device[5].name,"dev5_ip",device[5].ip,"dev6_name",device[6].name,"dev6_ip",device[6].ip,
                          "dev7_name",device[7].name,"dev7_ip",device[7].ip,"dev8_name",device[8].name,"dev8_ip",device[8].ip,
                          "dev9_name",device[9].name,"dev9_ip",device[9].ip,"dev10_name",device[10].name,"dev10_ip",device[10].ip,
                          "dev11_name",device[11].name,"dev11_ip",device[11].ip,"dev12_name",device[12].name,"dev12_ip",device[12].ip,
                          "dev13_name",device[13].name,"dev13_ip",device[13].ip,"dev14_name",device[14].name,"dev14_ip",device[14].ip,
                          "dev15_name",device[15].name,"dev15_ip",device[15].ip,"dev16_name",device[16].name,"dev16_ip",device[16].ip,
                          "dev17_name",device[17].name,"dev17_ip",device[17].ip,"dev18_name",device[18].name,"dev18_ip",device[18].ip,
                          "dev19_name",device[19].name,"dev19_ip",device[19].ip,"dev20_name",device[20].name,"dev20_ip",device[20].ip);
}

static PyObject *contabit_getstat(PyObject *self, PyObject *args)
{
    int req_err=0;
//    int ind_req=0, ind_key=0;
//    char **req=(char**)calloc(88,sizeof(char*)), **key=(char**)calloc(88,sizeof(char*));
//    u_long value[88];
    char *req;
    char build_string[282], request_time[44];
    struct tm *rt;
    time_t req_time;

    PyArg_ParseTuple(args, "|z",&req);

//    PyArg_ParseTuple(args, "|zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz",
//                     &req[0],&req[1],&req[2],&req[3],&req[4],&req[5],
//                     &req[6],&req[7],&req[8],&req[9],&req[10],&req[11],
//                     &req[12],&req[13],&req[14],&req[15],&req[16],&req[17],
//                     &req[18],&req[19],&req[20],&req[21],&req[22],&req[23],
//                     &req[24],&req[25],&req[26],&req[27],&req[28],&req[29]);
//
//    TEST:
//
//    while(req[ind_req]!=NULL)
//    {
//        printf("\n[C]  REQ %i: %s",ind_req,req[ind_req]);
//        ind_req++;
//    }
//
//    ind_req=0;

    req_time=time(0);
    rt=localtime(&req_time);
    strftime(request_time, sizeof request_time, "%a %Y/%m/%d %H:%M:%S", (const struct tm *) rt);

//    if (req[0]!=NULL)
//    {
//        ind_key=0; while(key[ind_key]!=NULL){key[ind_key]=NULL;ind_key++;}
//
//        ind_key=0;
//
//        while (req[ind_req]!=NULL)
//        {
//            if(strcmp(req[ind_req],"pcap")==0)
//            {
//                key[ind_key]="pkt_pcap_tot"; value[ind_key]=mystat.pkt_pcap_tot; ind_key++;
//                key[ind_key]="pkt_pcap_drop"; value[ind_key]=mystat.pkt_pcap_drop; ind_key++;
//                key[ind_key]="pkt_pcap_dropif"; value[ind_key]=mystat.pkt_pcap_dropif; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"pkt")==0)
//            {
//                key[ind_key]="pkt_up_nem"; value[ind_key]=mystat.pkt_up_nem; ind_key++;
//                key[ind_key]="pkt_up_oth"; value[ind_key]=mystat.pkt_up_oth; ind_key++;
//                key[ind_key]="pkt_up_all"; value[ind_key]=mystat.pkt_up_all; ind_key++;
//
//                key[ind_key]="pkt_down_nem"; value[ind_key]=mystat.pkt_down_nem; ind_key++;
//                key[ind_key]="pkt_down_oth"; value[ind_key]=mystat.pkt_down_oth; ind_key++;
//                key[ind_key]="pkt_down_all"; value[ind_key]=mystat.pkt_down_all; ind_key++;
//
//                key[ind_key]="pkt_tot_nem"; value[ind_key]=mystat.pkt_tot_nem; ind_key++;
//                key[ind_key]="pkt_tot_oth"; value[ind_key]=mystat.pkt_tot_oth; ind_key++;
//                key[ind_key]="pkt_tot_all"; value[ind_key]=mystat.pkt_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"pkt_nem")==0)
//            {
//                key[ind_key]="pkt_up_nem"; value[ind_key]=mystat.pkt_up_nem; ind_key++;
//                key[ind_key]="pkt_down_nem"; value[ind_key]=mystat.pkt_down_nem; ind_key++;
//                key[ind_key]="pkt_tot_nem"; value[ind_key]=mystat.pkt_tot_nem; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"pkt_oth")==0)
//            {
//                key[ind_key]="pkt_up_oth"; value[ind_key]=mystat.pkt_up_oth; ind_key++;
//                key[ind_key]="pkt_down_oth"; value[ind_key]=mystat.pkt_down_oth; ind_key++;
//                key[ind_key]="pkt_tot_oth"; value[ind_key]=mystat.pkt_tot_oth; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"pkt_all")==0)
//            {
//                key[ind_key]="pkt_up_all"; value[ind_key]=mystat.pkt_up_all; ind_key++;
//                key[ind_key]="pkt_down_all"; value[ind_key]=mystat.pkt_down_all; ind_key++;
//                key[ind_key]="pkt_tot_all"; value[ind_key]=mystat.pkt_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"byte")==0)
//            {
//                key[ind_key]="byte_up_nem"; value[ind_key]=mystat.byte_up_nem; ind_key++;
//                key[ind_key]="byte_up_oth"; value[ind_key]=mystat.byte_up_oth; ind_key++;
//                key[ind_key]="byte_up_all"; value[ind_key]=mystat.byte_up_all; ind_key++;
//
//                key[ind_key]="byte_down_nem"; value[ind_key]=mystat.byte_down_nem; ind_key++;
//                key[ind_key]="byte_down_oth"; value[ind_key]=mystat.byte_down_oth; ind_key++;
//                key[ind_key]="byte_down_all"; value[ind_key]=mystat.byte_down_all; ind_key++;
//
//                key[ind_key]="byte_tot_nem"; value[ind_key]=mystat.byte_tot_nem; ind_key++;
//                key[ind_key]="byte_tot_oth"; value[ind_key]=mystat.byte_tot_oth; ind_key++;
//                key[ind_key]="byte_tot_all"; value[ind_key]=mystat.byte_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"byte_nem")==0)
//            {
//                key[ind_key]="byte_up_nem"; value[ind_key]=mystat.byte_up_nem; ind_key++;
//                key[ind_key]="byte_down_nem"; value[ind_key]=mystat.byte_down_nem; ind_key++;
//                key[ind_key]="byte_tot_nem"; value[ind_key]=mystat.byte_tot_nem; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"byte_oth")==0)
//            {
//                key[ind_key]="byte_up_oth"; value[ind_key]=mystat.byte_up_oth; ind_key++;
//                key[ind_key]="byte_down_oth"; value[ind_key]=mystat.byte_down_oth; ind_key++;
//                key[ind_key]="byte_tot_oth"; value[ind_key]=mystat.byte_tot_oth; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"byte_all")==0)
//            {
//                key[ind_key]="byte_up_all"; value[ind_key]=mystat.byte_up_all; ind_key++;
//                key[ind_key]="byte_down_all"; value[ind_key]=mystat.byte_down_all; ind_key++;
//                key[ind_key]="byte_tot_all"; value[ind_key]=mystat.byte_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"payload")==0)
//            {
//                key[ind_key]="payload_up_nem"; value[ind_key]=mystat.payload_up_nem; ind_key++;
//                key[ind_key]="payload_up_oth"; value[ind_key]=mystat.payload_up_oth; ind_key++;
//                key[ind_key]="payload_up_all"; value[ind_key]=mystat.payload_up_all; ind_key++;
//
//                key[ind_key]="payload_down_nem"; value[ind_key]=mystat.payload_down_nem; ind_key++;
//                key[ind_key]="payload_down_oth"; value[ind_key]=mystat.payload_down_oth; ind_key++;
//                key[ind_key]="payload_down_all"; value[ind_key]=mystat.payload_down_all; ind_key++;
//
//                key[ind_key]="payload_tot_nem"; value[ind_key]=mystat.payload_tot_nem; ind_key++;
//                key[ind_key]="payload_tot_oth"; value[ind_key]=mystat.payload_tot_oth; ind_key++;
//                key[ind_key]="payload_tot_all"; value[ind_key]=mystat.payload_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"payload_nem")==0)
//            {
//                key[ind_key]="payload_up_nem"; value[ind_key]=mystat.payload_up_nem; ind_key++;
//                key[ind_key]="payload_down_nem"; value[ind_key]=mystat.payload_down_nem; ind_key++;
//                key[ind_key]="payload_tot_nem"; value[ind_key]=mystat.payload_tot_nem; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"payload_oth")==0)
//            {
//                key[ind_key]="payload_up_oth"; value[ind_key]=mystat.payload_up_oth; ind_key++;
//                key[ind_key]="payload_down_oth"; value[ind_key]=mystat.payload_down_oth; ind_key++;
//                key[ind_key]="payload_tot_oth"; value[ind_key]=mystat.payload_tot_oth; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"payload_all")==0)
//            {
//                key[ind_key]="payload_up_all"; value[ind_key]=mystat.payload_up_all; ind_key++;
//                key[ind_key]="payload_down_all"; value[ind_key]=mystat.payload_down_all; ind_key++;
//                key[ind_key]="payload_tot_all"; value[ind_key]=mystat.payload_tot_all; ind_key++;
//            }
//
//            else if(strcmp(req[ind_req],"pkt_pcap_tot")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_pcap_tot; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_pcap_drop")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_pcap_drop; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_pcap_dropif")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_pcap_dropif; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_up_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_up_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_up_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_up_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_up_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_up_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_down_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_down_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_down_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_down_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_down_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_down_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_tot_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_tot_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_tot_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_tot_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"pkt_tot_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.pkt_tot_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_up_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_up_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_up_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_up_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_up_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_up_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_down_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_down_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_down_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_down_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_down_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_down_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_tot_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_tot_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_tot_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_tot_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"byte_tot_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.byte_tot_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_up_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_up_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_up_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_up_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_up_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_up_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_down_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_down_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_down_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_down_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_down_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_down_all; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_tot_nem")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_tot_nem; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_tot_oth")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_tot_oth; ind_key++; }
//
//            else if(strcmp(req[ind_req],"payload_tot_all")==0)
//            { key[ind_key]=req[ind_req]; value[ind_key]=mystat.payload_tot_all; ind_key++; }
//
//            else
//            { key[ind_key]=req[ind_req]; value[ind_key]=-1; req_err--; ind_key++; }
//
//            ind_req++;
//        }
//
//        strcpy(build_string,"{s:s,s:i,");
//
//        ind_key=0;
//
//        while(key[ind_key]!=NULL)
//        {
//            strcat(build_string,",s:l");
//            //printf("Key %i: %s\tValue %i: %li\tReq %i: %s\n",ind_key,key[ind_key],ind_key,value[ind_key],ind_key,req[ind_key]);  //TEST
//            ind_key++;
//        }
//
//        strcat(build_string,"}");
//
//        return Py_BuildValue(build_string,
//                             "stat_time",request_time,"req_err",req_err,
//                             key[0],value[0],key[1],value[1],key[2],value[2],key[3],value[3],key[4],value[4],
//                             key[5],value[5],key[6],value[6],key[7],value[7],key[8],value[8],key[9],value[9],
//                             key[10],value[10],key[11],value[11],key[12],value[12],key[13],value[13],key[14],value[14],
//                             key[15],value[15],key[16],value[16],key[17],value[17],key[18],value[18],key[19],value[19],
//                             key[20],value[20],key[21],value[21],key[22],value[22],key[23],value[23],key[24],value[24],
//                             key[25],value[25],key[26],value[26],key[27],value[27],key[28],value[28],key[29],value[29],
//                             key[30],value[30],key[31],value[31],key[32],value[32],key[33],value[33],key[34],value[34],
//                             key[35],value[35],key[36],value[36],key[37],value[37],key[38],value[38],key[39],value[39]);
//    }

    strcpy(build_string,"{s:s,s:i,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l,s:l}");

    return Py_BuildValue(build_string,
                         "stat_time",request_time,"req_err",req_err,
                         "pkt_pcap_tot",mystat.pkt_pcap_tot,"pkt_pcap_drop",mystat.pkt_pcap_drop,"pkt_pcap_dropif",mystat.pkt_pcap_dropif,
                         "pkt_up_nem",mystat.pkt_up_nem,"pkt_up_oth",mystat.pkt_up_oth,"pkt_up_all",mystat.pkt_up_all,
                         "pkt_down_nem",mystat.pkt_down_nem,"pkt_down_oth",mystat.pkt_down_oth,"pkt_down_all",mystat.pkt_down_all,
                         "pkt_tot_nem",mystat.pkt_tot_nem,"pkt_tot_oth",mystat.pkt_tot_oth,"pkt_tot_all",mystat.pkt_tot_all,
                         "byte_up_nem",mystat.byte_up_nem,"byte_up_oth",mystat.byte_up_oth,"byte_up_all",mystat.byte_up_all,
                         "byte_down_nem",mystat.byte_down_nem,"byte_down_oth",mystat.byte_down_oth,"byte_down_all",mystat.byte_down_all,
                         "byte_tot_nem",mystat.byte_tot_nem,"byte_tot_oth",mystat.byte_tot_oth,"byte_tot_all",mystat.byte_tot_all,
                         "payload_up_nem",mystat.payload_up_nem,"payload_up_oth",mystat.payload_up_oth,"payload_up_all",mystat.payload_up_all,
                         "payload_down_nem",mystat.payload_down_nem,"payload_down_oth",mystat.payload_down_oth,"payload_down_all",mystat.payload_down_all,
                         "payload_tot_nem",mystat.payload_tot_nem,"payload_tot_oth",mystat.payload_tot_oth,"payload_tot_all",mystat.payload_tot_all);
}

static PyObject *contabit_geterr(PyObject *self)
{
    return Py_BuildValue("s",err_str);
}

static PyMethodDef contabit_methods[] =
{
    { "getdev", (PyCFunction)contabit_getdev, METH_VARARGS, NULL},
    { "initialize", (PyCFunction)contabit_initialize, METH_VARARGS, NULL},
    { "start", (PyCFunction)contabit_start, METH_NOARGS, NULL},
    { "stop", (PyCFunction)contabit_stop, METH_NOARGS, NULL},
    { "getstat", (PyCFunction)contabit_getstat, METH_VARARGS, NULL},
    { "geterr", (PyCFunction)contabit_geterr, METH_NOARGS, NULL},
    { NULL, NULL, 0, NULL }
};

void initcontabit(void)
{
    Py_InitModule("contabit", contabit_methods);
}