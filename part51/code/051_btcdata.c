/* Chapter 50: the table of embedded blocks (see 051_btcdata_data.asm). For the real blocks the hash is the one PUBLISHED in Bitcoin Core's test data (testnet3) or in the genesis block's own history (mainnet); for the synthetic chain it is the hash recorded when the block was mined. */
#include "051_btcdata.h"
extern const char sample_b_tn_0_start[], sample_b_tn_0_end[];
extern const char sample_b_tn_2_start[], sample_b_tn_2_end[];
extern const char sample_b_tn_3_start[], sample_b_tn_3_end[];
extern const char sample_b_tn_15007_start[], sample_b_tn_15007_end[];
extern const char sample_b_tn_49291_start[], sample_b_tn_49291_end[];
extern const char sample_b_tn_180480_start[], sample_b_tn_180480_end[];
extern const char sample_b_tn_926485_start[], sample_b_tn_926485_end[];
extern const char sample_b_tn_987876_start[], sample_b_tn_987876_end[];
extern const char sample_b_tn_1263442_start[], sample_b_tn_1263442_end[];
extern const char sample_b_tn_1414221_start[], sample_b_tn_1414221_end[];
extern const char sample_b_mn_0_start[], sample_b_mn_0_end[];
extern const char sample_b_syn_1_start[], sample_b_syn_1_end[];
extern const char sample_b_syn_2_start[], sample_b_syn_2_end[];
extern const char sample_b_syn_3_start[], sample_b_syn_3_end[];
extern const char sample_b_syn_4_start[], sample_b_syn_4_end[];
extern const char sample_b_syn_5_start[], sample_b_syn_5_end[];
extern const char sample_b_syn_6_start[], sample_b_syn_6_end[];
extern const char sample_b_syn_7_start[], sample_b_syn_7_end[];
extern const char sample_b_syn_8_start[], sample_b_syn_8_end[];
extern const char sample_b_syn_9_start[], sample_b_syn_9_end[];
extern const char sample_b_syn_10_start[], sample_b_syn_10_end[];
extern const char sample_b_syn_11_start[], sample_b_syn_11_end[];
extern const char sample_b_syn_12_start[], sample_b_syn_12_end[];
extern const char sample_b_syn_dup_start[], sample_b_syn_dup_end[];
extern const char sample_txvec_start[], sample_txvec_end[];
const btc_blk_t g_btc_blocks[N_BTC_BLOCKS] = {
    {"tn_0", "testnet", 0, "000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943", 0x1d00ffffu, sample_b_tn_0_start, sample_b_tn_0_end},
    {"tn_2", "testnet", 2, "000000006c02c8ea6e4ff69651f7fcde348fb9d557a06e6957b65552002a7820", 0x1d00ffffu, sample_b_tn_2_start, sample_b_tn_2_end},
    {"tn_3", "testnet", 3, "000000008b896e272758da5297bcd98fdc6d97c9b765ecec401e286dc1fdbe10", 0x1d00ffffu, sample_b_tn_3_start, sample_b_tn_3_end},
    {"tn_15007", "testnet", 15007, "0000000038c44c703bae0f98cdd6bf30922326340a5996cc692aaae8bacf47ad", 0x1d00ffffu, sample_b_tn_15007_start, sample_b_tn_15007_end},
    {"tn_49291", "testnet", 49291, "0000000018b07dca1b28b4b5a119f6d6e71698ce1ed96f143f54179ce177a19c", 0x1d00ffffu, sample_b_tn_49291_start, sample_b_tn_49291_end},
    {"tn_180480", "testnet", 180480, "00000000fd3ceb2404ff07a785c7fdcc76619edc8ed61bd25134eaa22084366a", 0x1d00ffffu, sample_b_tn_180480_start, sample_b_tn_180480_end},
    {"tn_926485", "testnet", 926485, "000000000000015d6077a411a8f5cc95caf775ccf11c54e27df75ce58d187313", 0x1d00ffffu, sample_b_tn_926485_start, sample_b_tn_926485_end},
    {"tn_987876", "testnet", 987876, "0000000000000c00901f2049055e2a437c819d79a3d54fd63e6af796cd7b8a79", 0x1d00ffffu, sample_b_tn_987876_start, sample_b_tn_987876_end},
    {"tn_1263442", "testnet", 1263442, "000000006f27ddfe1dd680044a34548f41bed47eba9e6f0b310da21423bc5f33", 0x1d00ffffu, sample_b_tn_1263442_start, sample_b_tn_1263442_end},
    {"tn_1414221", "testnet", 1414221, "0000000000000027b2b3b3381f114f674f481544ff2be37ae3788d7e078383b1", 0x1d00ffffu, sample_b_tn_1414221_start, sample_b_tn_1414221_end},
    {"mn_0", "mainnet", 0, "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f", 0x1d00ffffu, sample_b_mn_0_start, sample_b_mn_0_end},
    {"syn_1", "synthetic", 1, "000046701a4073bb63f3273bb3bba45bae4246fab335af820cf87da47936720a", 0x207fffffu, sample_b_syn_1_start, sample_b_syn_1_end},
    {"syn_2", "synthetic", 2, "00000ab441c2e3c0f4a49b93a821f4d0cdb34ebabd552325d4f461232d4693a0", 0x207fffffu, sample_b_syn_2_start, sample_b_syn_2_end},
    {"syn_3", "synthetic", 3, "0000ee1240493032e3c4f219c020e4198e78814bc9e7988e225c637b6c590f03", 0x207fffffu, sample_b_syn_3_start, sample_b_syn_3_end},
    {"syn_4", "synthetic", 4, "00005d3ef12fd7bfca9f061a7ea00ba5554298bd357f14ca10c51e3e43548f8d", 0x207fffffu, sample_b_syn_4_start, sample_b_syn_4_end},
    {"syn_5", "synthetic", 5, "000086f0bbad73d1a063bb81cff584e2c19efd3ec0c08a439de6f188e78bcf09", 0x207fffffu, sample_b_syn_5_start, sample_b_syn_5_end},
    {"syn_6", "synthetic", 6, "0000e39f4ce35b0c3db9c6f099b2a8dd38686e44abe304abd341f37d5ce8ab70", 0x207fffffu, sample_b_syn_6_start, sample_b_syn_6_end},
    {"syn_7", "synthetic", 7, "000024c10198a63635062386c9c26086ee5fa72a8bef7fbd2feb7385743b770a", 0x207fffffu, sample_b_syn_7_start, sample_b_syn_7_end},
    {"syn_8", "synthetic", 8, "000025df2dfee2dbd1d256b701b29a71dce3f37dc52dcc1873cc93eade2624ac", 0x207fffffu, sample_b_syn_8_start, sample_b_syn_8_end},
    {"syn_9", "synthetic", 9, "000076abaa6fc660d19be6f9fa328806164f897cbd3b8293d6d05de0a854cb2b", 0x207fffffu, sample_b_syn_9_start, sample_b_syn_9_end},
    {"syn_10", "synthetic", 10, "0000cb11c96568a58cf43e8a9abf1d039ae25d8c253d8449fb4adae9e7c951f1", 0x207fffffu, sample_b_syn_10_start, sample_b_syn_10_end},
    {"syn_11", "synthetic", 11, "0000c663ee5bc17d1187e6f5a59c4bc662c7753c8e65b72b1a7b54df3363d6fd", 0x207fffffu, sample_b_syn_11_start, sample_b_syn_11_end},
    {"syn_12", "synthetic", 12, "00005153603acca4447c5a141f84f437471a0c2c6794b876ba9f5bcaf3ef6800", 0x207fffffu, sample_b_syn_12_start, sample_b_syn_12_end},
    {"syn_dup", "synthetic-dup", 13, "0000a832d9c8a69b3d001484a1dbdabfaf3d41f323f4fcd6887180bbc15204e8", 0x207fffffu, sample_b_syn_dup_start, sample_b_syn_dup_end},
};
const char *const g_btc_txvec_start = sample_txvec_start;
const char *const g_btc_txvec_end = sample_txvec_end;
