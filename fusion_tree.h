#include <immintrin.h>
#include <cstdint>

#define MAX_FUSION_SIZE 16

using namespace std; //maybe stop doing this

typedef struct {
	uint8_t size;
	uint8_t remaining[7];
} fusion_metadata;

typedef struct {
	__m256i treebits;
	//we want to extract 16 bits from 512 bit key, so we have one mask that extracts the bytes that hold the keys, and then we do 2 cycles of pext to get the bits we need
	__mmask64 byte_extract;
	uint64_t bitextract[2]; //we have to keep these in order, and for maintaining the least endian order that cpu uses (we sometimes treat this as a single number to do ops on, we actually say that the bitextract[0] deals with the *higher* bits. Should this actually just be stored as an _mm128i? We could even popcnt in parallel, then just need to add the two pieces.
	 //idk what to do with this but this struct remains 64 bits from a 512 bit struct so this could store stuff about children which could speed up balancing
	fusion_metadata meta;
} fusion_tree;

/*typedef struct fast_insert_tree {
	fustion_tree tree;
	_m256i ignore_mask;
}*/

//for now implementing this fast fusion tree as default, but maybe segment fusion tree into two pieces, cause they have different ways of searching, inserting
typedef struct {
	fusion_tree tree;
	__m256i ignore_mask; //idk what exactly to do with this
	__m128i key_positions; //convert key position assuming sorted array to the real position in the keys array
	//also maybe add a vector for the free_positions to support deletions easily
	__m512i keys[MAX_FUSION_SIZE];
} fusion_node;

/*typedef struct fast_fusion_node {
	
}*/

uint64_t get_uint64_from_m256(__m256i vec, int pos);
uint64_t get_uint64_from_m512(__m512i vec, int pos);

int first_diff_bit_pos(__m512i x, __m512i y);

int get_bit_from_pos(__m256i key, int pos);
int get_bit_from_pos(__m512i key, int pos);

__m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit = 1);

bool full(fusion_node* node);

uint16_t extract_bits(fusion_tree* tree, __m512i key);

__m256i compare_mask(fusion_node* node, uint16_t basemask);

int search_position(fusion_node* node, uint16_t basemask, const bool geq=true);
int search_pos_arr(fusion_node* node, uint16_t basemask, const bool geq=true);
int search_pos_tree(fusion_node* node, uint16_t basemask);
int search_partial_pos_tree(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest);

__m512i get_key_from_sorted_pos(fusion_node* node, int index_in_sorted);
__m512i search_key(fusion_node* node, uint16_t basemask);

//void update_mask(fusion_node* node, int bitpos);

int diff_bit_to_mask_pos(fusion_node* node, unsigned int diffpos);

__m256i insert_mask(__m256i maskvec, uint16_t mask, int pos, bool right); //insert 16 bit mask into some position (specifying which mask)

__m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right);

__m256i shift_mask(__m256i maskvec, int pos); //moves the bits to the left of pos one to the left to make room for a new bit there. pos is specifying bit in mask.

uint16_t shift_maskling(uint16_t mask, int pos);

void add_position_to_extraction_mask(fusion_tree* tree, int pos_in_key);

int insert(fusion_node* node, __m512i key);

int query_branch(fusion_node* node, __m512i key);

