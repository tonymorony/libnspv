/*

 The MIT License (MIT)

 Copyright (c) 2017 Jonas Schnelli

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 
*/

#include "libbtc-config.h"

#include <btc/chainparams.h>
#include <btc/ecc.h>
#include <btc/net.h>
#include <btc/netspv.h>
#include <btc/protocol.h>
#include <btc/random.h>
#include <btc/serialize.h>
#include <btc/tool.h>
#include <btc/tx.h>
#include <btc/utils.h>
#include <btc/wallet.h>

#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nSPV_defs.h>

static struct option long_options[] =
    {
        {"testnet", no_argument, NULL, 't'},
        {"regtest", no_argument, NULL, 'r'},
        {"ips", no_argument, NULL, 'i'},
        {"debug", no_argument, NULL, 'd'},
        {"maxnodes", no_argument, NULL, 'm'},
        {"dbfile", no_argument, NULL, 'f'},
        {"continuous", no_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}};

static void print_version()
{
    printf("Version: %s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

static void print_usage()
{
    print_version();
    printf("Usage: nspv (-c|continuous) (-i|-ips <ip,ip,...]>) (-m[--maxpeers] <int>) (-t[--testnet]) (-f <headersfile|0 for in mem only>) (-r[--regtest]) (-d[--debug]) (-s[--timeout] <secs>) <command>\n");
    printf("Supported commands:\n");
    printf("        scan      (scan blocks up to the tip, creates header.db file)\n");
    printf("\nExamples: \n");
    printf("Sync up to the chain tip and stores all headers in headers.db (quit once synced):\n");
    printf("> nspv scan\n\n");
    printf("Sync up to the chain tip and give some debug output during that process:\n");
    printf("> nspv -d scan\n\n");
    printf("Sync up, show debug info, don't store headers in file (only in memory), wait for new blocks:\n");
    printf("> nspv -d -f 0 -c scan\n\n");
}

static bool showError(const char* er)
{
    printf("Error: %s\n", er);
    return 1;
}

btc_bool spv_header_message_processed(struct btc_spv_client_ *client, btc_node *node, btc_blockindex *newtip) {
    UNUSED(client);
    UNUSED(node);
    if (newtip) {
        printf("New headers tip height %d\n", newtip->height);
    }
    return true;
}

static btc_bool quit_when_synced = true;
void spv_sync_completed(btc_spv_client* client) {
    printf("Sync completed, at height %d\n", client->headers_db->getchaintip(client->headers_db_ctx)->height);
    if (quit_when_synced) {
        btc_node_group_shutdown(client->nodegroup);
    }
    else {
        printf("Waiting for new blocks or relevant transactions...\n");
    }
}

#include "nSPV_utils.h"
#include "nSPV_structs.h"
#include "nSPV_superlite.h"
#include "komodo_cJSON.c"
#include "nSPV_rpc.h"

/*
 Todo:
 HDRhash
 addr message
 login and address/passphrase handling
 JSON chainparams, maybe use coins repo
 */

int main(int argc, char* argv[])
{
    int ret = 0;
    int long_index = 0;
    int opt = 0;
    char* data = 0;
    char* ips = 0;
    btc_bool debug = false;
    int timeout = 15;
    int maxnodes = 10;
    char* dbfile = 0;
    const btc_chainparams *chain = &nspv_chainparams_main;
    portable_mutex_init(&NSPV_commandmutex);
    if ( argc > 1 )
    {
        if ( strcmp(argv[1],"KMD") == 0 )
        {
            chain = &kmd_chainparams_main;
            argc--;
            argv++;
        }
        else if ( strcmp(argv[1],"BTC") == 0 )
        {
            chain = &btc_chainparams_main;
            argc--;
            argv++;
        }
    }
    if (chain->komodo == 0 && (argc <= 1 || strlen(argv[argc - 1]) == 0 || argv[argc - 1][0] == '-'))
    {
        // exit if no command was provided
        print_usage();
        exit(EXIT_FAILURE);
    }
    data = argv[argc - 1];
    strcpy(NSPV_symbol,chain->name);
    // get arguments
    while ((opt = getopt_long_only(argc, argv, "i:ctrds:m:f:", long_options, &long_index)) != -1) {
        switch (opt) {
        case 'c':
            quit_when_synced = false;
            break;
        case 't':
            chain = &btc_chainparams_test;
            break;
        case 'r':
            chain = &btc_chainparams_regtest;
            break;
        case 'd':
            debug = true;
            break;
        case 's':
            timeout = (int)strtol(optarg, (char**)NULL, 10);
            break;
        case 'i':
            ips = optarg;
            break;
        case 'm':
            maxnodes = (int)strtol(optarg, (char**)NULL, 10);
            break;
        case 'f':
            dbfile = optarg;
            break;
        case 'v':
            print_version();
            exit(EXIT_SUCCESS);
            break;
        default:
            print_usage();
            exit(EXIT_FAILURE);
        }
    }
    uint16_t port = chain->rpcport;
    if ( OS_thread_create(malloc(sizeof(pthread_t)),NULL,NSPV_rpcloop,(void *)&port) != 0 )
    {
        printf("error launching NSPV_rpcloop for port.%u\n",port);
        exit(-1);
    }
    if ( chain->komodo != 0 )
    {
        int32_t i; uint256 revhash;
        if ( ips == 0 )
            ips = (char *)chain->dnsseeds[0].domain;
        for (i=0; i<(int32_t)sizeof(revhash); i++)
        {
            revhash[i] = chain->genesisblockhash[31 - i];
            fprintf(stderr,"%02x",chain->genesisblockhash[31 - i]);
        }
        memcpy((void *)chain->genesisblockhash,revhash,sizeof(chain->genesisblockhash));
        fprintf(stderr," genesisblockhash %s\n",chain->name);
        data = (char *)"scan";
    }
    if (strcmp(data, "scan") == 0)
    {
        char walletname[64],headersname[64]; int32_t i,error,res;
        sprintf(walletname,"wallet.%s",chain->name);
        sprintf(headersname,"headers.%s",chain->name);
        btc_ecc_start();
        btc_spv_client* client = btc_spv_client_new(chain, debug, (dbfile && (dbfile[0] == '0' || (strlen(dbfile) > 1 && dbfile[0] == 'n' && dbfile[0] == 'o'))) ? true : false);
        NSPV_client = client;
        if ( chain->nSPV == 0 )
        {
            btc_wallet *wallet = btc_wallet_new(chain);
            btc_bool created;
            res = btc_wallet_load(wallet,walletname,&error,&created);
            if (!res)
            {
                fprintf(stdout, "Loading %s failed error.%d\n",walletname,error);
                exit(EXIT_FAILURE);
            }
            if ( created != 0 )
            {
                // create a new key
                btc_hdnode node;
                uint8_t seed[32];
                assert(btc_random_bytes(seed,sizeof(seed),true));
                btc_hdnode_from_seed(seed,sizeof(seed),&node);
                btc_wallet_set_master_key_copy(wallet,&node);
            }
            else
            {
                // ensure we have a key
                fprintf(stderr,"TODO: ensure there is a key\n");
            }
            
            btc_wallet_hdnode* node = btc_wallet_next_key(wallet);
            size_t strsize = 128;
            char str[strsize];
            btc_hdnode_get_p2pkh_address(node->hdnode,chain,str,strsize);
            printf("%s Wallet addr: %s (child %d)\n", chain->name,str, node->hdnode->child_num);
            vector *addrs = vector_new(1,free);
            btc_wallet_get_addresses(wallet,addrs);
            for (i=0; i<(int32_t)addrs->len; i++)
            {
                char* addr= vector_idx(addrs, i);
                printf("Addr: %s\n", addr);
            }
            vector_free(addrs, true);
            client->sync_completed = spv_sync_completed;
            client->sync_transaction = btc_wallet_check_transaction;
            client->sync_transaction_ctx = wallet;
        }
        client->header_message_processed = spv_header_message_processed;
        if ( chain->nSPV == 0 && !btc_spv_client_load(client, (dbfile ? dbfile : headersname)))
        {
            printf("Could not load or create %s database...aborting\n",headersname);
            ret = EXIT_FAILURE;
        }
        else
        {
            fprintf(stderr,"Discover %s peers...",chain->name);
            btc_spv_client_discover_peers(client,ips);
            //fprintf(stderr,"done\n");
            //printf("Connecting to the p2p network...\n");
            btc_spv_client_runloop(client);
            printf("end of client runloop\n");
            btc_spv_client_free(client);
            ret = EXIT_SUCCESS;
        }
        btc_ecc_stop();
    }
    else
    {
        printf("Invalid command (use -?)\n");
        ret = EXIT_FAILURE;
    }
    return ret;
}
