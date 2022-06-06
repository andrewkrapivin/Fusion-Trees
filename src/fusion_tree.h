#ifndef FUSION_TREE_H_INCLUDED
#define FUSION_TREE_H_INCLUDED

#include <immintrin.h>
#include <cstdint>

#define MAX_FUSION_SIZE 16

using namespace std; //maybe stop doing this. Ok definitely stop doing

typedef struct {
	uint8_t size;
	bool fast;
	uint8_t remaining[4];
	uint16_t fullkey; //each bit says whether a particular key is a "full" key, or that the key stored here stands for an entire key rather than a part of a key. Basically if has an associated val
} fusion_metadata;

typedef struct {
	__m256i treebits;
	//we want to extract 16 bits from 512 bit key, so we have one mask that extracts the bytes that hold the keys, and then we do 2 cycles of pext to get the bits we need
	__mmask64 byte_extract;
	uint64_t bitextract[2]; //we have to keep these in order, and for maintaining the least endian order that cpu uses (we sometimes treat this as a single number to do ops on, we actually say that the bitextract[0] deals with the *higher* bits. Should this actually just be stored as an _mm128i? We could even popcnt in parallel, then just need to add the two pieces.
	 //idk what to do with this but this struct remains 64 bits from a 512 bit struct so this could store stuff about children which could speed up balancing
	fusion_metadata meta;
} fusion_tree;

extern const fusion_tree Empty_Fusion_Tree;

/*typedef struct fast_insert_tree {
	fustion_tree tree;
	_m256i ignore_mask;
}*/

//for now implementing this fast fusion tree as default, but maybe segment fusion tree into two pieces, cause they have different ways of searching, inserting
typedef struct {
	__m512i keys[MAX_FUSION_SIZE];
	//also maybe add a vector for the free_positions to support deletions easily
	//__m128i uselessdata;
	fusion_tree tree;
	__m256i ignore_mask; //idk what exactly to do with this
	__m128i key_positions; //convert key position assuming sorted array to the real position in the keys array
} fusion_node;

extern const fusion_node Empty_Fusion_Node;

/*typedef struct fast_fusion_node {
	
}*/

fusion_node* new_empty_fusion_node();

//are tehse even neccesary? Am I being ridiculous with all those instructions? Maybe better to just get vec[pos] like that?	
inline uint64_t get_uint64_from_m256(__m256i vec, int pos);
inline uint64_t get_uint64_from_m512(__m512i vec, int pos);

// #define first_diff_bit_pos(x, y) my_first_diff_bit_pos(x, y, __FILE__, __LINE__)
//int first_diff_bit_pos(__m512i x, __m512i y); // seems works
int first_diff_bit_pos(__m512i x, __m512i y);

inline int get_bit_from_pos(__m256i key, int pos);
inline int get_bit_from_pos(__m512i key, int pos);

bool fast_compare__m512i(__m512i a, __m512i b);
bool compare__m512i(__m512i a, __m512i b);

inline __m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit = 1);

bool node_full(fusion_node* node);

inline uint16_t extract_bits(fusion_tree* tree, __m512i key); // seems works

inline __m256i compare_mask(fusion_node* node, uint16_t basemask);

inline int search_position(fusion_node* node, uint16_t basemask, const bool geq=true);
inline int search_pos_arr(fusion_node* node, uint16_t basemask, const bool geq=true);
inline int search_pos_tree(fusion_node* node, uint16_t basemask);
inline int search_partial_pos_tree(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest);
inline int search_partial_pos_tree2(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest);

uint8_t get_real_pos_from_sorted_pos(fusion_node* node, int index_in_sorted);
__m512i get_key_from_sorted_pos(fusion_node* node, int index_in_sorted);
__m512i search_key(fusion_node* node, uint16_t basemask);

//void update_mask(fusion_node* node, int bitpos);

inline int diff_bit_to_mask_pos(fusion_node* node, unsigned int diffpos);

inline __m256i insert_mask(__m256i maskvec, uint16_t mask, int pos, bool right); //insert 16 bit mask into some position (specifying which mask)
inline __m256i insert_partial_duplicate_mask(__m256i maskvec, int pos, bool right, int cuttoff_pos);

inline __m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right);

inline __m256i shift_mask(__m256i maskvec, int pos); //moves the bits to the left of pos one to the left to make room for a new bit there. pos is specifying bit in mask.

inline uint16_t shift_maskling(uint16_t mask, int pos);

inline void add_position_to_extraction_mask(fusion_tree* tree, int pos_in_key); // seems works

int insert(fusion_node* node, __m512i key);

int query_branch(fusion_node* node, __m512i key); //for now returns the bitwise complement if key is found

inline int search_pos_tree_fast(fusion_node* node, uint16_t basemask);

int query_branch_fast(fusion_node* node, __m512i key);

void make_fast(fusion_node* node, bool sort = true);

int insert_fast(fusion_node* node, __m512i key);

// #define query_branch_node(x, y) my_query_branch_node(x, y, __FILE__, __LINE__)
//int query_branch_node(fusion_node* node, __m512i key);
int query_branch_node(fusion_node* node, __m512i key);
int insert_key_node(fusion_node* node, __m512i key);

void print_keys_sig_bits(fusion_node* node);

// void split_u64_matched_to_keys(std::pair<fusion_node*, uint64_t*> source, std::pair<fusion_node*, uint64_t*> low, std::pair<fusion_node*, uint64_t*> high); //TODO

#endif 
