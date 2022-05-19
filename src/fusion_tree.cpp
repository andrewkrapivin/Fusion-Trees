#include "fusion_tree.h"
#include "HelperFuncs.h"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <cassert>

extern const fusion_node Empty_Fusion_Node = {0};

fusion_node* new_empty_fusion_node() {
	fusion_node* new_node = static_cast<fusion_node*>(std::aligned_alloc(64, sizeof(fusion_node)));
    memset(new_node, 0, sizeof(fusion_node));
    return new_node;
}

inline uint64_t get_uint64_from_m256(__m256i vec, int pos) {
	__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
	__m256i movinglongtofront = _mm256_maskz_compress_epi64(pos_mask, vec);
	__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
	uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
	return lowestlong;
}
inline uint64_t get_uint64_from_m512(__m512i vec, int pos) {
	__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
	__m512i movinglongtofront = _mm512_maskz_compress_epi64(pos_mask, vec);
	__m128i lowerbytes = _mm512_extracti64x2_epi64(movinglongtofront, 0);
	uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
	return lowestlong;
}

int first_diff_bit_pos(__m512i x, __m512i y) {//there is def some confusion here with me about big endian/little endian so maybe need to change this a bit
	//cout << "File: " << file << ", line: " << line << endl;
	__m512i z = _mm512_xor_si512(x, y);
	/*for(int i = 0; i < 8; i++) {
		cout << z[i] << " "; 
	}
	cout << endl;*/
	__m512i w = _mm512_xor_si512(z, z);
	__mmask16 nonzero_pieces = _mm512_cmp_epi32_mask(z, w, _MM_CMPINT_NE);
	unsigned int nzr = _cvtmask16_u32(nonzero_pieces);
	if(nzr == 0) return -1; //-1 means the numbers are the same

	//we are looking for the "leading" zeros, but we want to reverse to get the bit position cause all of this stuff makes absolutely no sense
	unsigned int significant_one_index = 31 - _lzcnt_u32(nzr); // the naming of these functions is horrible! Because it counts trailizing zeros, but that's assuming big endian! But trailizing zeros should assume little endian, cause that's what the cpu sees! This causes even more confusion, as I'm thinking of implementing little endian I know also have to reverse my thinking here with the name
	//cout << significant_one_index << endl;
	__mmask16 significant_one_mask = _cvtu32_mask16(1 << significant_one_index);
	//__m128i nzbytes = _mm512_extracti64x2_epi64(z, significant_one_index/2);
	//uint64_t diffbyte = _mm_extract_epi64(nzbytes, significant_one_index%2);
	w = _mm512_maskz_compress_epi32(significant_one_mask, z); //must be a better way to do this
	__m128i lowerbytes = _mm512_extracti64x2_epi64(w, 0);
	//unsigned int diffint = _mm512_cvtsi512_si32(w);
	unsigned int diffint = _mm_extract_epi32(lowerbytes, 0);
	return 32*significant_one_index + (31 - _lzcnt_u32(diffint));
}

inline int get_bit_from_pos(__m512i key, int pos) {//check if pos out of bounds?
	/*int vecindex = pos/64;
	__mmask8 vecindex_mask = _cvtu32_mask8(1 << vecindex);
	__m512i movinglongtofront = _mm512_maskz_compress_epi64(vecindex_mask, key);
	__m128i lowerbytes = _mm512_extracti64x2_epi64(movinglongtofront, 0);
	uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
	int longpos = pos%64;
	return (lowestlong & (1 << longpos)) != 0;*/
	/*int vecindex = pos/32;
	__mmask16 vecindex_mask = _cvtu32_mask16(1 << vecindex);
	__m512i movinglongtofront = _mm512_maskz_compress_epi32(vecindex_mask, key);
	uint32_t lowestint = _mm512_cvtsi512_si32(movinglongtofront);
	int longpos = pos%32;
	return (lowestlong & (1 << longpos)) != 0;*/
	uint64_t element_containing_bit = get_uint64_from_m512(key, pos/64);
	//print_binary_uint64(element_containing_bit, true);
	return (element_containing_bit & (1ull << (pos%64))) != 0;
}

inline int get_bit_from_pos(__m256i key, int pos) {//check if pos out of bounds?
	/*int vecindex = pos/32;
	__mmask8 vecindex_mask = _cvtu32_mask8(1 << vecindex);
	__m256i movinglongtofront = _mm256_maskz_compress_epi32(vecindex_mask, key);
	uint32_t lowestint = _mm256_cvtsi256_si32(movinglongtofront);
	int longpos = pos%32;
	return (lowestlong & (1 << longpos)) != 0;*/
	uint64_t element_containing_bit = get_uint64_from_m256(key, pos/64);
	return (element_containing_bit & (1ull << (pos%64))) != 0;
}

//returns -1 if x=y, 0 if first diff bit of x is zero, 1 otherwise. This kind of nonstandard thing just to fit right into what I had b4
inline int fast_first_diff_bit_val(__m512i x, __m512i y) {
	// cout << "Comparing two numbers" << endl;
	// print_binary_uint64_big_endian(x[7], true, 64, 8);
    // print_binary_uint64_big_endian(y[7], true, 64, 8);
	__mmask16 ne_mask = _mm512_cmp_epu32_mask(x, y, _MM_CMPINT_NE);
	__mmask16 ge_mask = _mm512_cmp_epu32_mask(x, y, _MM_CMPINT_GE);
	unsigned short ne = _cvtmask16_u32(ne_mask);
	unsigned short ge = _cvtmask16_u32(ge_mask);
	// cout << "The two important stuffs. ge: " << (ge & (1ull << 15)) << ", ne: " << (ne & (1ull << 15))<< endl;
	unsigned int first_ne = 31 - _lzcnt_u32(ne);
	// cout << "first ne: " << first_ne << endl; 
	return ne == 0 ? -1 : ((ge & (1 << first_ne)) != 0);
}

bool fast_compare__m512i(__m512i a, __m512i b) {
	return fast_first_diff_bit_val(a, b) == 0;
}

bool compare__m512i(__m512i a, __m512i b) { //remember for some reason compare in sort is assumed to evaluate a < b
	int diffbitpos = first_diff_bit_pos(a, b);
	//cout << "Comparing two nums:" << endl;
	//print_vec(a, true);
	//print_vec(b, true);
	//int x = get_bit_from_pos(b, diffbitpos);
	//cout << "Diffbitpos: " << diffbitpos << ", and the actual bit: " << x << endl;
	if(diffbitpos == -1) return false;
	return get_bit_from_pos(b, diffbitpos);
}

inline __m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit /* = 1*/) {
	__mmask16 setmask = _cvtu32_mask16((1 << (high+1)) - (1 << low));
	__m256i addvec = _mm256_maskz_set1_epi16(setmask, bit << epi16pos);
	return _mm256_or_si256(src, addvec);
}

bool node_full(fusion_node* node){
	return node->tree.meta.size == MAX_FUSION_SIZE;
}

//wow big endian vs little endian really really sucks, but I think this function works
inline uint16_t extract_bits(fusion_tree* tree, __m512i key) {
	__m512i extractedbytes = _mm512_maskz_compress_epi8(tree->byte_extract, key);
	__m128i lowerbytes = _mm512_extracti64x2_epi64(extractedbytes, 0);
	uint64_t ebyteslong[2];
	ebyteslong[0] = _mm_extract_epi64(lowerbytes, 0);
	ebyteslong[1] = _mm_extract_epi64(lowerbytes, 1); //better way to do this?
	return (_pext_u64(ebyteslong[1], tree->bitextract[1]) << _mm_popcnt_u64(tree->bitextract[0])) + _pext_u64(ebyteslong[0], tree->bitextract[0]);
}

inline __m256i compare_mask(fusion_node* node, uint16_t basemask) {
	__m256i cmpmask = _mm256_set1_epi16(basemask);
	return _mm256_and_si256(cmpmask, node->ignore_mask);
}

inline int search_position(fusion_node* node, uint16_t basemask, const bool geq /*= true*/) {
	__m256i cmpmask = compare_mask(node, basemask);
	__mmask16 geq_mask;
	/*if(geq)
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LE);
	else
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LT);*/
	geq_mask = geq ? _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LE): _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LT);
	uint16_t converted = _cvtmask16_u32(geq_mask);
	//we need to ignore comparisons with stuff where its beyond the size!!!!
	//cout << "SFDSF " << ((1 << node->tree.meta.size) - 1) << endl;
	return _mm_popcnt_u32(converted & ((1 << node->tree.meta.size) - 1));
}

//returns the position in the array of the largest element less than or equal to the basemask
inline int search_pos_arr(fusion_node* node, uint16_t basemask, const bool geq /*= true*/) {
	int pos = search_position(node, basemask, geq);
	pos = max (pos-1, 0);
	return pos;
}
//returns the position in the array of the element which would be reached by going down the blind trie using the basemask
inline int search_pos_tree(fusion_node* node, uint16_t basemask) {
	int pos_arr = search_pos_arr(node, basemask);
	//cout << "Pos_arr is " << pos_arr << endl;
	//basically we want to see whether the pos_arr or pos_arr+1 matches the basemask more closely, cause pos_arr and pos_arr+1 obviously difffer in one bit and we want to see which "path" or branch basemask takes at that differing bit
	//clearly, if we've reached the rightmost node of the tree, we are done
	if(pos_arr == node->tree.meta.size-1) return pos_arr;
	__mmask16 pos_mask = _cvtu32_mask16((1 << pos_arr) * 3); //we want the position of this element and the next position of course for our comparison
	__m256i movinglongtofront = _mm256_maskz_compress_epi16(pos_mask, _mm256_and_si256(node->tree.treebits, node->ignore_mask));
	__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
	uint32_t lowestint = _mm_extract_epi32(lowerbytes, 0);
	//now we want the element which more closely matches, so the one with the smaller first differing bit
	const uint32_t otherkeypos = 1 << 16;
	lowestint ^= ((uint32_t) basemask) * (otherkeypos + 1); //xor each part to get first differing bit with each compressed key. We then just need to see which part is smaller!
	return ((lowestint%otherkeypos) < (lowestint/otherkeypos)) ? pos_arr : (pos_arr+1);
}

//basically finds either the smallest or largest leaf of an "internal node" in the tree--or here really the largest or smallest key that matches the basemask up to cutoff_pos
inline int search_partial_pos_tree(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest) {
	uint16_t partial_basemask = basemask & (~((1 << cutoff_pos) - 1)); //remember to use ~ not ! for bitwise lol
	partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
	//cout << "cutoff_pos is " << cutoff_pos << ", partial basemask is " << partial_basemask << " FDSFSD " << search_pos_arr(node, partial_basemask) <<  endl;
	return search_pos_arr(node, partial_basemask);
}

inline int search_partial_pos_tree2(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest) {
	uint16_t partial_basemask = basemask & (~((1 << cutoff_pos) - 1)); //remember to use ~ not ! for bitwise lol
	partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
	//cout << "cutoff_pos is " << cutoff_pos << ", partial basemask is " << partial_basemask << " FDSFSD " << search_pos_arr(node, partial_basemask) <<  endl;
	return search_position(node, partial_basemask, largest);
}
uint8_t get_real_pos_from_sorted_pos(fusion_node* node, int index_in_sorted) {
	if(node->tree.meta.fast) return index_in_sorted;
	__mmask16 pos_mask = _cvtu32_mask16((1 << index_in_sorted)); //we want the position of this element and the next position of course for our comparison
	__m128i extracting_position = _mm_maskz_compress_epi8(pos_mask, node->key_positions);
	uint8_t position = _mm_extract_epi8(extracting_position, 0);
	return position;
}

__m512i get_key_from_sorted_pos(fusion_node* node, int index_in_sorted) {
	if(node->tree.meta.fast) return node->keys[index_in_sorted];
	__mmask16 pos_mask = _cvtu32_mask16((1 << index_in_sorted)); //we want the position of this element and the next position of course for our comparison
	__m128i extracting_position = _mm_maskz_compress_epi8(pos_mask, node->key_positions);
	uint8_t position = _mm_extract_epi8(extracting_position, 0);
	return node->keys[position];
}

__m512i search_key(fusion_node* node, uint16_t basemask) {
	int pos = search_position(node, basemask);
	pos = max (pos-1, 0);
	return get_key_from_sorted_pos(node, pos);
}

inline int diff_bit_to_mask_pos(fusion_node* node, unsigned int diffpos) {
	uint64_t byte_extract_ll = _cvtmask64_u64(node->tree.byte_extract);
	unsigned int diffposbyte = diffpos/8;
	bool test_specific_bits = byte_extract_ll & (1ll << diffposbyte);
	int numbelow = _mm_popcnt_u64(byte_extract_ll & ((1ll << diffposbyte) - 1));
	//cout << "diffposbyte: " << diffposbyte << ", numbelow " << numbelow << ", test specific bits " << test_specific_bits << ", byte_extract_ll " << byte_extract_ll << endl;
	int maskpos = 0;
	if(numbelow > 0) {
		uint64_t tmp = min(numbelow, 8) * 8;
		//cout << ((1ll << tmp) - 1ll) << ", " << (1ll << tmp) << ", " << tmp <<  endl; // um why is (1ll << 64) equal to 1 that makes absolutely no sense
		uint64_t extract_mask = tmp == 64 ? (- 1ll) : ((1ll << tmp) - 1ll);
		maskpos += _mm_popcnt_u64(node->tree.bitextract[0] & extract_mask);
	}
	if(numbelow > 8) {
		uint64_t tmp = min(numbelow-8, 8) * 8;
		//uint64_t extract_mask = tmp == 64 ? (- 1ll) : ((1ll << tmp) - 1ll);
		maskpos += _mm_popcnt_u64(node->tree.bitextract[1] & ((1ll << tmp) - 1ll));
		//maskpos += _mm_popcnt_u64(node->tree.bitextract[1] & extract_mask);
	}
	if(test_specific_bits) {
		uint8_t tmp = *((uint8_t*)node->tree.bitextract + numbelow);
		maskpos += _mm_popcnt_u64(tmp & ((1 << ((diffpos%8) + 1)) - 1));
		if(tmp & (1 << (diffpos%8)))
			maskpos *= -1;
	}
	return maskpos;
}

/*void update_mask(fusion_node* node, int bitpos) {
	
}*/

//Right specifies where the value is inserted. Is it inserted to the right of the mask at the desired position or to the left?
inline __m256i insert_mask(__m256i maskvec, uint16_t mask, int pos, bool right) {
	//cout << "inserting mask: " << mask << " " << pos << " " << right << endl;
	//print_vec(maskvec, true, 16);
	if(right)
		pos++;
	uint16_t expand_mask_bits = (-1) ^ (1 << pos);
	__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
	maskvec = _mm256_maskz_expand_epi16(expand_mask, maskvec);
	//cout << "maskvec" << endl;
	//print_vec(maskvec, true, 16);
	__m256i addmask = _mm256_maskz_set1_epi16(_knot_mask16(expand_mask), mask);
	maskvec = _mm256_or_si256(addmask, maskvec);
	//print_vec(maskvec, true, 16);
	return maskvec;
}

inline __m256i insert_partial_duplicate_mask(__m256i maskvec, int pos, bool right, int cuttoff_pos) {
	//cout << "inserting mask: " << mask << " " << pos << " " << right << endl;
	//print_vec(maskvec, true, 16);
	uint16_t expand_mask_bits = (-1) ^ (1 << pos);
	__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
	__m256i maskvec_exp = _mm256_maskz_expand_epi16(expand_mask, maskvec);
	__m256i ans = _mm256_mask_blend_epi16(expand_mask_bits, maskvec, maskvec_exp);

	expand_mask = _knot_mask16(expand_mask);
	if(right) expand_mask = _kshiftli_mask16(expand_mask, 1);
	__m256i cuttoffmask = _mm256_maskz_set1_epi16(expand_mask, (1 << cuttoff_pos) - 1);
	//cout << "maskvec" << endl;
	//print_vec(maskvec, true, 16);
	return _mm256_andnot_si256(cuttoffmask, ans);
}

inline __m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right) {
	if(!right)
		sorted_pos++;
	uint16_t expand_mask_bits = (-1) ^ (1 << sorted_pos);
	__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
	key_positions = _mm_maskz_expand_epi8(expand_mask, key_positions);
	__m128i addpos = _mm_maskz_set1_epi8(_knot_mask16(expand_mask), real_pos);
	key_positions = _mm_or_si128(addpos, key_positions);
	return key_positions;
}

inline __m256i shift_mask(__m256i maskvec, int pos) {
	uint16_t keep_mask = (1 << pos) - 1;
	__mmask16 ones = 0xffff;
	__m256i lowerbits_mask = _mm256_maskz_set1_epi16(ones, keep_mask);
	//cout << "set1epi16 lower check: " << endl;
	//print_vec(lowerbits_mask, true, 16);
	__m256i lowerbits = _mm256_and_si256(maskvec, lowerbits_mask);
	//cout << "lower bits check: " << endl;
	//print_vec(lowerbits, true, 16);
	__m256i higherbits = _mm256_andnot_si256(lowerbits_mask, maskvec);
	//cout << "higher first check: " << endl;
	//print_vec(higherbits, true, 16);
	constexpr __m128i count = {1, 0};
	higherbits = _mm256_maskz_sll_epi16(ones, higherbits, count);
	//cout << "higher second check: " << endl;
	//print_vec(higherbits, true, 16);
	return _mm256_or_si256(lowerbits, higherbits);
}

inline uint16_t shift_maskling(uint16_t mask, int pos) {
	uint16_t lowerbits_mask = (1 << pos) - 1;
	return (mask & lowerbits_mask) + ((mask & (~lowerbits_mask)) << 1); //classic ! vs ~
}

inline void add_position_to_extraction_mask(fusion_tree* tree, int pos_in_key) {
	//update the byte_extract mask
	int byte_in_key = pos_in_key/8;
	__mmask64 addbit_to_byte_extract_mask = _cvtu64_mask64(1ull << byte_in_key);
	__mmask64 new_byte_extract = _kor_mask64(tree->byte_extract, addbit_to_byte_extract_mask);
	
	uint64_t byte_extract_ll = _cvtmask64_u64(tree->byte_extract);
	int numbelow = _mm_popcnt_u64(byte_extract_ll & ((1ll << byte_in_key) - 1));
	//cout << "num below: " << numbelow << endl;
	//if we changed byte extract, then we need to shift the bit extract over by one byte, which we can do with avx. Otherwise we just need to add the corresponding bit to bit extract
	if(!_kortestz_mask64_u8(_kxor_mask64(new_byte_extract, tree->byte_extract), 0)) { //kortest gives one if the or is all zeros, but we want the two numbers to be different and thus not all zeros
		//cout << "SHIFTING STUFF" << endl;
		__m128i bitextract_to_shift = _mm_lddqu_si128((const __m128i*)&tree->bitextract[0]);
		__mmask16 expand_mmask = _cvtu32_mask16 (~(1 << numbelow));
		//cout << (~(1 << numbelow)) << " " << expand_mmask << " " << tree->bitextract[0] << endl;
		_mm_storeu_si128((__m128i*)&tree->bitextract[0], _mm_maskz_expand_epi8(expand_mmask, bitextract_to_shift));
		//cout << (~(1 << numbelow)) << " " << expand_mmask << " " << tree->bitextract[0] << endl;
	}
	tree->bitextract[numbelow/8] |= (1ull << (((numbelow%8) * 8) + pos_in_key%8));
	//cout << tree->bitextract[0] << endl;
	tree->byte_extract = new_byte_extract;
}

//add option for when the tree is empty! Cause then we really don't want to update anything, just add the key!
int insert(fusion_node* node, __m512i key) {
	if(node->tree.meta.size == 0) { //Should we assume that key_positions[0] is zero?
		//cout << "SDLJFLKSDJF" << endl;
		node->keys[0] = key;
		//cout << "SDLJFLKSDJF" << endl;
		node->tree.meta.size++;
		return 1;
	}

	if(node->tree.meta.size == MAX_FUSION_SIZE)
		return -1;
		
	uint16_t first_basemask = extract_bits(&node->tree, key);
	
	int first_guess_pos = search_pos_tree(node, first_basemask);

	//cout << "The extracted mask is " << first_basemask << ", and the guess position is " << first_guess_pos << endl;
	
	__m512i first_guess = get_key_from_sorted_pos(node, first_guess_pos);
	
	int diff_bit_pos = first_diff_bit_pos(first_guess, key);
	//cout << "Diff bit pos is " << diff_bit_pos << endl;
	
	if (diff_bit_pos == -1) { //key is already in there
		return -2;
	}
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	int search_mask_pos = mask_pos;
	//cout << "mask pos is " << mask_pos <<  endl;
	
	int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
	
	bool need_highest = !diff_bit_val; //if the diff_bit_val is one, then we already have the highest element that fits the mask until this point!
	
	if (mask_pos >= 0) { //then we do not already have this bit in the "fusion tree." We thus need to shift our masks and stuff to include this bit
		node->tree.treebits = shift_mask(node->tree.treebits, mask_pos);
		//cout << "Tree bits after shift" << endl;
		//print_vec(node->tree.treebits, true, 16);
		//cout << "Ignore mask before shift" << endl;
		//print_vec(node->ignore_mask, true, 16);
		node->ignore_mask = shift_mask(node->ignore_mask, mask_pos);
		//cout << "Ignore mask after shift" << endl;
		//print_vec(node->ignore_mask, true, 16);
		//and we also shift the mask itself! And set the corresponding bit to the corresponding value
		first_basemask = shift_maskling(first_basemask, mask_pos);
		first_basemask |= diff_bit_val << mask_pos;
		//cout << "modified first_basemask is " << first_basemask << endl;
		
		add_position_to_extraction_mask(&node->tree, diff_bit_pos);
		
		//increment search_mask_pos cause we want to search all the elements that are in the same subtree, but the bit defining the subtree has now increased, since we added a so far nonsense bit into the masks
		search_mask_pos ++;
		//cout << "Search mask pos: " << search_mask_pos << endl;
		//if its an unseen bit then we need to query first_guess_pos again to find where the actual insert position should be. Obviously this is pretty bad code, so fix this.
	}
	else {
		//probably not the best way to do this, but well simply just like if we have seen mask_pos, then well we dodn't want to affect stuff at the maskpos position cause that's actually where our last important bit is. Otherwise we did actually move everything one over
		mask_pos = -1 - mask_pos;
	}

	first_guess_pos = search_partial_pos_tree(node, first_basemask, mask_pos+1, !need_highest);
	
	int low_pos = need_highest ? first_guess_pos : search_partial_pos_tree(node, first_basemask, mask_pos+1, false);
	
	int high_pos = need_highest ? search_partial_pos_tree(node, first_basemask, mask_pos+1, true) : first_guess_pos;
	//cout << first_guess_pos << endl;
	//cout << low_pos << " " << high_pos << " " << need_highest << " " << first_basemask << endl;
	
	//we need to set ignore mask to not ignore positions here
	node->ignore_mask = setbit_each_epi16_in_range(node->ignore_mask, mask_pos, low_pos, high_pos);
	
	//we need to set ignore mask to not ignore positions here
	node->tree.treebits = setbit_each_epi16_in_range(node->tree.treebits, mask_pos, low_pos, high_pos, 1-diff_bit_val);
	
	//we need to insert the treebits in but we need to be careful about whether basically we are shifting the element at the first_guess_pos left or right, and we want to shift it based on whether our node is bigger or smaller than the element! So if the diffbit is one, we want to shift it left (smaller), and otherwise we shift right (bigger), which means 
	node->tree.treebits = insert_mask(node->tree.treebits, first_basemask, first_guess_pos, diff_bit_val);
	
	//here I am inserting just don't ignore any bits, but I am not 100% sure that is correct. Is easier to do this than copying the guy we're next to, but that might be necessary. Def check this
	//node->ignore_mask = insert_mask(node->ignore_mask, -1, first_guess_pos, diff_bit_val); //lol fixing insert_mask took way longer than it should have
	//Pretty sure I came up with a situation where the above is wrong, but didn't work it out. Just being safe then:
	//cout << "Duplicating mask " << first_guess_pos << " in ignore mask: " << endl;
	//cout << "Before: "; print_vec(node->ignore_mask, true, 16);
	//actually we want to duplicate it only up to the bit which matters, so the mask_pos. Here I am really not sure its important either, but well let's be safe
	node->ignore_mask = insert_partial_duplicate_mask(node->ignore_mask, first_guess_pos, diff_bit_val, mask_pos);
	//cout << "After: "; print_vec(node->ignore_mask, true, 16);

	//cause the mask we inserted for our new element might have extra bits set that it should not have
	node->tree.treebits = _mm256_and_si256(node->tree.treebits, node->ignore_mask);
	
	node->key_positions = insert_sorted_pos_to_key_positions(node->key_positions, first_guess_pos, node->tree.meta.size, !diff_bit_val); //idk where to move it left or right for now but just say it moves to the left
	
	node->keys[node->tree.meta.size] = key;
	
	node->tree.meta.size++;
	
	return 0;
}

int query_branch(fusion_node* node, __m512i key) {
	uint16_t first_basemask = extract_bits(&node->tree, key);
	
	int first_guess_pos = search_pos_tree(node, first_basemask);
	
	__m512i first_guess = get_key_from_sorted_pos(node, first_guess_pos);
	
	int diff_bit_pos = first_diff_bit_pos(first_guess, key);

	//cout << "Query guess: " << first_guess_pos << ", basemask: " << first_basemask << ", diff_bit_pos: " << diff_bit_pos << endl;
	
	if (diff_bit_pos == -1) { //key is already in there
		return ~first_guess_pos; //idk if its negative then let's say you've found the exact key
	}
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
	int second_guess_pos = search_partial_pos_tree2(node, first_basemask, abs(mask_pos), diff_bit_val);
	return second_guess_pos;
}

inline int search_position_fast(fusion_node* node, uint16_t basemask, const bool geq /*= true*/) {
	__m256i cmpmask = _mm256_set1_epi16(basemask);
	__mmask16 geq_mask;
	/*if(geq)
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LE);
	else
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LT);*/
	geq_mask = geq ? _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LE): _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LT);
	uint16_t converted = _cvtmask16_u32(geq_mask);
	//we need to ignore comparisons with stuff where its beyond the size!!!!
	//cout << "SFDSF " << ((1 << node->tree.meta.size) - 1) << endl;
	return _mm_popcnt_u32(converted & ((1 << node->tree.meta.size) - 1));
}

//returns the position in the array of the largest element less than or equal to the basemask
inline int search_pos_arr_fast(fusion_node* node, uint16_t basemask, const bool geq = true) {
	int pos = search_position_fast(node, basemask, geq);
	pos = max (pos-1, 0);
	return pos;
}

//returns the position in the array of the element which would be reached by going down the blind trie using the basemask
inline int search_pos_tree_fast(fusion_node* node, uint16_t basemask) {
	int pos_arr = search_pos_arr_fast(node, basemask);
	//cout << "Pos_arr is " << pos_arr << endl;
	//basically we want to see whether the pos_arr or pos_arr+1 matches the basemask more closely, cause pos_arr and pos_arr+1 obviously difffer in one bit and we want to see which "path" or branch basemask takes at that differing bit
	//clearly, if we've reached the rightmost node of the tree, we are done
	if(pos_arr == node->tree.meta.size-1) return pos_arr;
	__mmask16 pos_mask = _cvtu32_mask16((1 << pos_arr) * 3); //we want the position of this element and the next position of course for our comparison
	__m256i movinglongtofront = _mm256_maskz_compress_epi16(pos_mask, node->tree.treebits);
	__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
	uint32_t lowestint = _mm_extract_epi32(lowerbytes, 0);
	//now we want the element which more closely matches, so the one with the smaller first differing bit
	const uint32_t otherkeypos = 1 << 16;
	lowestint ^= ((uint32_t) basemask) * (otherkeypos + 1); //xor each part to get first differing bit with each compressed key. We then just need to see which part is smaller!
	return ((lowestint%otherkeypos) < (lowestint/otherkeypos)) ? pos_arr : (pos_arr+1);
}

inline int search_partial_pos_tree2_fast(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest) {
	uint16_t partial_basemask = basemask & (~((1 << cutoff_pos) - 1)); //remember to use ~ not ! for bitwise lol
	partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
	//cout << "cutoff_pos is " << cutoff_pos << ", partial basemask is " << partial_basemask << " FDSFSD " << search_pos_arr(node, partial_basemask) <<  endl;
	return search_position_fast(node, partial_basemask, largest);
}

int query_branch_fast(fusion_node* node, __m512i key) {
	uint16_t first_basemask = extract_bits(&node->tree, key);
	
	int first_guess_pos = search_pos_tree_fast(node, first_basemask);
	
	//__m512i first_guess = get_key_from_sorted_pos(node, first_guess_pos);
	__m512i first_guess = node->keys[first_guess_pos];
	
	//print_vec(key, true);
	//cout << first_guess_pos << " adsfasdf " << node->tree.meta.size << endl;
	int diff_bit_pos = first_diff_bit_pos(first_guess, key);
	//cout << "diff bit pos: " << diff_bit_pos << endl;

	//cout << "Query guess: " << first_guess_pos << ", basemask: " << first_basemask << ", diff_bit_pos: " << diff_bit_pos << endl;
	
	if (diff_bit_pos == -1) { //key is already in there
		return ~first_guess_pos; //idk if its negative then let's say you've found the exact key
	}
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
	int second_guess_pos = search_partial_pos_tree2_fast(node, first_basemask, abs(mask_pos), diff_bit_val);
	//cout << "SD:LKFJSD:LKFJ:LSDKF:LSDKJ" << endl;
	return second_guess_pos;
}

void make_fast(fusion_node* node, bool sort /* = true */) {
	// return;
	if(node->tree.meta.size == 0) {
		node->tree.meta.fast = true;
		return;
	}
	assert(node->tree.meta.size != 0);
	//cout << "Making fast w/ " << node->tree.meta.size << endl;
	if(sort) {
		std::sort(node->keys, node->keys+node->tree.meta.size, fast_compare__m512i);
		// cout << (int)node->tree.meta.size << endl;
		// for(int i=0; i<node->tree.meta.size; i++)
		// 	//print_binary_uint64_big_endian(node->keys[i][7], false, 8, 8);
		// 	print_vec(node->keys[i], true);
		// cout << endl;
	}
	// cout << "CLD" << endl;
	// print_binary_uint64(node->tree.bitextract[0], true);
	for(int i=0; i<node->tree.meta.size; i++) {
		// print_vec(node->tree.treebits, true, 16);
		uint16_t sketch = extract_bits(&node->tree, node->keys[i]);
		// print_binary_uint64(sketch, true);
		node->tree.treebits = insert_mask(node->tree.treebits, sketch, i, false);
		// print_vec(node->tree.treebits, true, 16);
	}
	// print_vec(node->tree.treebits, true, 16);
	node->tree.meta.fast = true;
}

//Not actually fast lol. Just for inserting into the node that has fast search
int insert_fast(fusion_node* node, __m512i key) {
	if(node->tree.meta.size == 0) { //Should we assume that key_positions[0] is zero?
		//cout << "SDLJFLKSDJF" << endl;
		node->keys[0] = key;
		//cout << "SDLJFLKSDJF" << endl;
		node->tree.meta.size++;
		return 1;
	}

	if(node->tree.meta.size == MAX_FUSION_SIZE)
		return -1;
	int i = 0;

	uint16_t first_sketch = extract_bits(&node->tree, key);
	int first_guess_pos = search_pos_tree_fast(node, first_sketch);
	
	int diff_bit_pos = first_diff_bit_pos(node->keys[first_guess_pos], key);
	if (diff_bit_pos == -1) {
		return -2;
	}
	//cout << "diff_bit_pos " << diff_bit_pos << endl;
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	
	if (mask_pos >= 0) { //then we do not already have this bit in the "fusion tree." We thus need to shift our masks and stuff to include this bit
		add_position_to_extraction_mask(&node->tree, diff_bit_pos);
	}

	node->keys[node->tree.meta.size] = key;
	
	node->tree.meta.size++;
	// print_keys_sig_bits(node);

	make_fast(node, true);
	return 0;
}

int query_branch_node(fusion_node* node, __m512i key) {
	if(node->tree.meta.fast) {
		return query_branch_fast(node, key);
	}
	return query_branch(node, key);
}

int insert_key_node(fusion_node* node, __m512i key) {
	if(node->tree.meta.fast) {
		return insert_fast(node, key);
	}
	return insert(node, key);
}

void print_keys_sig_bits(fusion_node* node) {
	for(int i=0; i<node->tree.meta.size; i++) {
		print_binary_uint64_big_endian(get_key_from_sorted_pos(node, i)[7], true, 64, 16);
	}
}
