
/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef KOMODO_NSPV_DEFSH
#define KOMODO_NSPV_DEFSH

#include <time.h>
#include <pthread.h>
#include <btc/netspv.h>

union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;

#define SATOSHIDEN ((uint64_t)100000000)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define portable_mutex_t pthread_mutex_t
#define portable_mutex_init(ptr) pthread_mutex_init(ptr,NULL)
#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock
#define OS_thread_create pthread_create
#define NSPV_MAXPACKETSIZE (4096 * 1024)

struct rpcrequest_info
{
    struct rpcrequest_info *next,*prev;
    pthread_t T;
    int32_t sock;
    uint32_t ipbits;
    uint16_t port,pad;
};

#include "komodo_cJSON.h"

#define NODE_NSPV (1 << 30)
#define NODE_ADDRINDEX (1 << 29)
#define NODE_SPENTINDEX (1 << 28)

#define NSPV_POLLITERS 100
#define NSPV_POLLMICROS 50000
#define NSPV_MAXVINS 64
#define NSPV_AUTOLOGOUT 777
#define NSPV_BRANCHID 0x76b809bb

// nSPV defines and struct definitions with serialization and purge functions

#define NSPV_INFO 0x00
#define NSPV_INFORESP 0x01
#define NSPV_UTXOS 0x02
#define NSPV_UTXOSRESP 0x03
#define NSPV_NTZS 0x04
#define NSPV_NTZSRESP 0x05
#define NSPV_NTZSPROOF 0x06
#define NSPV_NTZSPROOFRESP 0x07
#define NSPV_TXPROOF 0x08
#define NSPV_TXPROOFRESP 0x09
#define NSPV_SPENTINFO 0x0a
#define NSPV_SPENTINFORESP 0x0b
#define NSPV_BROADCAST 0x0c
#define NSPV_BROADCASTRESP 0x0d
#define NSPV_TXIDS 0x0e
#define NSPV_TXIDSRESP 0x0f
#define NSPV_MEMPOOL 0x10
#define NSPV_MEMPOOLRESP 0x11
#define NSPV_MEMPOOL_ALL 0
#define NSPV_MEMPOOL_ADDRESS 1
#define NSPV_MEMPOOL_ISSPENT 2
#define NSPV_MEMPOOL_INMEMPOOL 3
#define NSPV_MEMPOOL_CCEVALCODE 4

#define COIN SATOSHIDEN

struct NSPV_equihdr
{
    int32_t nVersion;
    bits256 hashPrevBlock;
    bits256 hashMerkleRoot;
    bits256 hashFinalSaplingRoot;
    uint32_t nTime;
    uint32_t nBits;
    bits256 nNonce;
    uint8_t nSolution[1344];
};

struct NSPV_utxoresp
{
    bits256 txid;
    int64_t satoshis,extradata;
    int32_t vout,height;
};

struct NSPV_utxosresp
{
    struct NSPV_utxoresp *utxos;
    char coinaddr[64];
    int64_t total,interest;
    int32_t nodeheight,skipcount,pad32;
    uint16_t numutxos,CCflag;
};

struct NSPV_txidresp
{
    bits256 txid;
    int64_t satoshis;
    int32_t vout,height;
};

struct NSPV_txidsresp
{
    struct NSPV_txidresp *txids;
    char coinaddr[64];
    int32_t nodeheight,skipcount,pad32;
    uint16_t numtxids,CCflag;
};

struct NSPV_mempoolresp
{
    bits256 *txids;
    char coinaddr[64];
    bits256 txid;
    int32_t nodeheight,vout,vindex;
    uint16_t numtxids; uint8_t CCflag,funcid;
};

struct NSPV_ntz
{
    bits256 blockhash,txid,othertxid;
    int32_t height,txidheight;
};

struct NSPV_ntzsresp
{
    struct NSPV_ntz prevntz,nextntz;
    int32_t reqheight;
};

struct NSPV_inforesp
{
    struct NSPV_ntz notarization;
    bits256 blockhash;
    int32_t height,hdrheight;
    struct NSPV_equihdr H;
};

struct NSPV_txproof
{
    bits256 txid;
    int64_t unspentvalue;
    int32_t height,vout,txlen,txprooflen;
    uint8_t *tx,*txproof;
};

struct NSPV_ntzproofshared
{
    struct NSPV_equihdr *hdrs;
    int32_t prevht,nextht,pad32;
    uint16_t numhdrs,pad16;
};

struct NSPV_ntzsproofresp
{
    struct NSPV_ntzproofshared common;
    bits256 prevtxid,nexttxid;
    int32_t prevtxidht,nexttxidht,prevtxlen,nexttxlen;
    uint8_t *prevntz,*nextntz;
};

struct NSPV_MMRproof
{
    struct NSPV_ntzproofshared common;
    // tbd
};

struct NSPV_spentinfo
{
    struct NSPV_txproof spent;
    bits256 txid;
    int32_t vout,spentvini;
};

struct NSPV_broadcastresp
{
    bits256 txid;
    int32_t retcode;
};

struct NSPV_CCmtxinfo
{
    struct NSPV_utxosresp U;
    struct NSPV_utxoresp used[NSPV_MAXVINS];
};

extern uint32_t NSPV_logintime,NSPV_lastinfo;
extern char NSPV_lastpeer[],NSPV_pubkeystr[],NSPV_wifstr[],NSPV_address[];
bits256 NSPV_hdrhash(struct NSPV_equihdr *hdr);
extern uint32_t NSPV_STOP_RECEIVED;

int32_t iguana_rwnum(const btc_chainparams *chain,int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
btc_node *NSPV_req(btc_spv_client *client,btc_node *node,uint8_t *msg,int32_t len,uint64_t mask,int32_t ind);
void NSPV_logout(void);
int32_t NSPV_periodic(btc_node *node);
void komodo_nSPVresp(btc_node *from,uint8_t *response,int32_t len);
bits256 bits256_doublesha256(uint8_t *data,int32_t datalen);
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
int32_t is_hexstr(char *str,int32_t n);

#endif // KOMODO_NSPV_DEFSH
