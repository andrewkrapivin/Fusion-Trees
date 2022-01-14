#include "fusion_tree.h"

#include <algorithm>
#include <iostream>

uint64_t get_uint64_from_m256(__m256i vec, int pos) {
	__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
	__m256i movinglongtofront = _mm256_maskz_compress_epi64(pos_mask, vec);
	__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
	uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
	return lowestlong;
}
uint64_t get_uint64_from_m512(__m512i vec, int pos) {
	__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
	__m512i movinglongtofront = _mm512_maskz_compress_epi64(pos_mask, vec);
	__m128i lowerbytes = _mm512_extracti64x2_epi64(movinglongtofront, 0);
	uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
	return lowestlong;
}

int first_diff_bit_pos(__m512i x, __m512i y) {//there is def some confusion here with me about big endian/little endian so maybe need to change this a bit
	__m512i z = _mm512_xor_si512(x, y);
	/*for(int i = 0; i < 8; i++) {
		cout << z[i] << " "; 
	}
	cout << endl;*/
	__m512i w = _mm512_xor_si512(z, z);
	__mmask16 nonzero_pieces = _mm512_cmp_epi32_mask(z, w, _MM_CMPINT_NE);
	unsigned int nzr = _cvtmask16_u32(nonzero_pieces);
	if(nzr == 0) return -1; //-1 means the numbers are the same
	unsigned int significant_one_index = _tzcnt_u32(nzr);



	
	//cout << significant_one_index << endl;
	__mmask16 significant_one_mask = _cvtu32_mask16(1 << significant_one_index);
	//__m128i nzbytes = _mm512_extracti64x2_epi64(z, significant_one_index/2);
	//uint64_t diffbyte = _mm_extract_epi64(nzbytes, significant_one_index%2);
	w = _mm512_maskz_compress_epi32(significant_one_mask, z); //must be a better way to do this
	__m128i lowerbytes = _mm512_extracti64x2_epi64(w, 0);
	//unsigned int diffint = _mm512_cvtsi512_si32(w);
	unsigned int diffint = _mm_extract_epi32(lowerbytes, 0);
	return 32*significant_one_index + (31-_lzcnt_u32(diffint));
}

int get_bit_from_pos(__m512i key, int pos) {//check if pos out of bounds?
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
	return (element_containing_bit & (1 << pos%64)) != 0;
}

int get_bit_from_pos(__m256i key, int pos) {//check if pos out of bounds?
	/*int vecindex = pos/32;
	__mmask8 vecindex_mask = _cvtu32_mask8(1 << vecindex);
	__m256i movinglongtofront = _mm256_maskz_compress_epi32(vecindex_mask, key);
	uint32_t lowestint = _mm256_cvtsi256_si32(movinglongtofront);
	int longpos = pos%32;
	return (lowestlong & (1 << longpos)) != 0;*/
	uint64_t element_containing_bit = get_uint64_from_m256(key, pos/64);
	return (element_containing_bit & (1 << pos%64)) != 0;
}

__m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit /* = 1*/) {
	__mmask16 setmask = _cvtu32_mask16((1 << (high+1)) - (1 << low));
	__m256i addvec = _mm256_maskz_set1_epi16(setmask, bit << epi16pos);
	return _mm256_or_si256(src, addvec);
}

bool full(fusion_node* node){
	return node->tree.meta.size == MAX_FUSION_SIZE;
}

//wow big endian vs little endian really really sucks, but I think this function works
uint16_t extract_bits(fusion_tree* tree, __m512i key) {
	__m512i extractedbytes = _mm512_maskz_compress_epi8(tree->byte_extract, key);
	__m128i lowerbytes = _mm512_extracti64x2_epi64(extractedbytes, 0);
	uint64_t ebyteslong[2];
	ebyteslong[0] = _mm_extract_epi64(lowerbytes, 0);
	ebyteslong[1] = _mm_extract_epi64(lowerbytes, 1); //better way to do this?
	return (_pext_u64(ebyteslong[1], tree->bitextract[1]) << _mm_popcnt_u64(tree->bitextract[0])) + _pext_u64(ebyteslong[0], tree->bitextract[0]);
}

__m256i compare_mask(fusion_node* node, uint16_t basemask) {
	__m256i cmpmask = _mm256_set1_epi16(basemask);
	return _mm256_and_si256(cmpmask, node->ignore_mask);
}

int search_position(fusion_node* node, uint16_t basemask, const bool geq /*= true*/) {
	__m256i cmpmask = compare_mask(node, basemask);
	__mmask16 geq_mask;
	if(geq)
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LE);
	else
		geq_mask = _mm256_cmp_epi16_mask(node->tree.treebits, cmpmask, _MM_CMPINT_LT);
	uint16_t converted = _cvtmask16_u32(geq_mask);
	//we need to ignore comparisons with stuff where its beyond the size!!!!

	return _mm_popcnt_u32(converted & ((1 << node->tree.meta.size) - 1));
}

//returns the position in the array of the largest element less than or equal to the basemask
int search_pos_arr(fusion_node* node, uint16_t basemask, const bool geq /*= true*/) {
	int pos = search_position(node, basemask, geq);
	pos = max (pos-1, 0);
	return pos;
}
//returns the position in the array of the element which would be reached by going down the blind trie using the basemask
//DEFINITELY ERROR: MAKE SURE TO WHEN COMPARING THE THINGS MANUALLY USE THE IGNORE MASK WITH BASEMASK FOR THE CORRESPONDING POSITIONS
int search_pos_tree(fusion_node* node, uint16_t basemask) {
	int pos_arr = search_pos_arr(node, basemask);
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

//basically finds either the smallest or largest leaf of an "internal node" in the tree--or here really the largest or smallest key that matches the basemask up to cutoff_pos
int search_partial_pos_tree(fusion_node* node, uint16_t basemask, int cutoff_pos, bool largest) {
	uint16_t partial_basemask = basemask & (!((1 << cutoff_pos) - 1));
	partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
	return search_pos_arr(node, basemask);
}

__m512i get_key_from_sorted_pos(fusion_node* node, int index_in_sorted) {
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

int diff_bit_to_mask_pos(fusion_node* node, unsigned int diffpos) {
	uint64_t byte_extract_ll = _cvtmask64_u64(node->tree.byte_extract);
	unsigned int diffposbyte = diffpos/8;
	bool test_specific_bits = byte_extract_ll & (1ll << diffposbyte);
	int numbelow = _mm_popcnt_u64(byte_extract_ll & ((1ll << diffposbyte) - 1));
	int maskpos = 0;
	if(numbelow > 0) {
		uint64_t tmp = min(numbelow, 8) * 8;
		maskpos += _mm_popcnt_u64(node->tree.bitextract[1] & ((1ll << tmp) - 1ll));
	}
	if(numbelow > 8) {
		uint64_t tmp = min(numbelow-8, 8) * 8;
		maskpos += _mm_popcnt_u64(node->tree.bitextract[0] & ((1ll << tmp) - 1ll));
	}
	if(test_specific_bits) {
		uint64_t tmp = *((uint8_t*)node->tree.bitextract + numbelow);
		maskpos += _mm_popcnt_u64(tmp & ((1 << (diffpos+1)) - 1));
		if(tmp & (1 << diffpos))
			maskpos *= -1;
	}
	return maskpos;
}

/*void update_mask(fusion_node* node, int bitpos) {
	
}*/

__m256i insert_mask(__m256i maskvec, uint16_t mask, int pos, bool right) {
	if(!right)
		pos++;
	uint16_t expand_mask_bits = (-1) ^ (1 << pos);
	__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
	maskvec = _mm256_maskz_expand_epi16(expand_mask, maskvec);
	__m256i addmask = _mm256_maskz_set1_epi16(_knot_mask16(expand_mask), mask);
	maskvec = _mm256_or_si256(addmask, maskvec);
	return maskvec;
}

__m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right) {
	if(!right)
		sorted_pos++;
	uint16_t expand_mask_bits = (-1) ^ (1 << sorted_pos);
	__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
	key_positions = _mm_maskz_expand_epi8(expand_mask, key_positions);
	__m128i addpos = _mm_maskz_set1_epi8(_knot_mask16(expand_mask), real_pos);
	key_positions = _mm_or_si128(addpos, key_positions);
	return key_positions;
}

__m256i shift_mask(__m256i maskvec, int pos) {
	uint16_t keep_mask = (1 << pos) - 1;
	__mmask16 ones = 0xffff;
	__m256i lowerbits_mask = _mm256_maskz_set1_epi16(ones, keep_mask);
	__m256i lowerbits = _mm256_and_si256(maskvec, lowerbits_mask);
	__m256i higherbits = _mm256_andnot_si256(lowerbits_mask, maskvec);
	__m128i count = _mm_set1_epi16(1);
	higherbits = _mm256_maskz_sll_epi16(ones, higherbits, count);
	return _mm256_and_si256(lowerbits, higherbits);
}

uint16_t shift_maskling(uint16_t mask, int pos) {
	uint16_t lowerbits_mask = (1 << pos) - 1;
	return (mask & lowerbits_mask) + ((mask & (!lowerbits_mask)) << 1);
}

void add_position_to_extraction_mask(fusion_tree* tree, int pos_in_key) {
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
		node->keys[0] = key;
		node->tree.meta.size++;
		return 1;
	}

	if(node->tree.meta.size == MAX_FUSION_SIZE)
		return -1;
		
	uint16_t first_basemask = extract_bits(&node->tree, key);
	
	int first_guess_pos = search_pos_tree(node, first_basemask);
	
	__m512i first_guess = get_key_from_sorted_pos(node, first_guess_pos);
	
	int diff_bit_pos = first_diff_bit_pos(first_guess, key);
	
	if (diff_bit_pos == -1) { //key is already in there
		return -2;
	}
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	
	int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
	
	bool need_highest = !diff_bit_val; //if the diff_bit_val is one, then we already have the highest element that fits the mask until this point!
	
	if (mask_pos >= 0) { //then we do not already have this bit in the "fusion tree." We thus need to shift our masks and stuff to include this bit
		node->tree.treebits = shift_mask(node->tree.treebits, mask_pos);
		node->ignore_mask = shift_mask(node->ignore_mask, mask_pos);
		//and we also shift the mask itself! And set the corresponding bit to the corresponding value
		first_basemask = shift_maskling(first_basemask, mask_pos);
		first_basemask |= diff_bit_val << mask_pos;

		cout << "CHECK CHECK" << endl;
		
		add_position_to_extraction_mask(&node->tree, diff_bit_pos);

		cout << "CHECK CHECK" << endl;
		
		//if its an unseen bit then we need to query first_guess_pos again to find where the actual insert position should be. Obviously this is pretty bad code, so fix this.
		first_guess_pos = search_partial_pos_tree(node, first_basemask, mask_pos, need_highest);
	}
	
	mask_pos = abs(mask_pos);
	
	int low_pos = need_highest ? first_guess_pos : search_partial_pos_tree(node, first_basemask, mask_pos, false);
	
	int high_pos = need_highest ? search_partial_pos_tree(node, first_basemask, mask_pos, true) : first_guess_pos;
	cout << first_guess_pos << endl;
	cout << low_pos << " " << high_pos << " " << need_highest << endl;
	
	//we need to set ignore mask to not ignore positions here
	node->ignore_mask = setbit_each_epi16_in_range(node->ignore_mask, mask_pos, low_pos, high_pos);
	
	//we need to set ignore mask to not ignore positions here
	node->tree.treebits = setbit_each_epi16_in_range(node->tree.treebits, mask_pos, low_pos, high_pos, 1-diff_bit_val);
	
	//we need to insert the treebits in but we need to be careful about whether basically we are shifting the element at the first_guess_pos left or right, and we want to shift it based on whether our node is bigger or smaller than the element! So if the diffbit is one, we want to shift it left (smaller), and otherwise we shift right (bigger), which means 
	node->tree.treebits = insert_mask(node->tree.treebits, first_basemask, first_guess_pos, !diff_bit_val);
	
	//here I am inserting just don't ignore any bits, but I am not 100% sure that is correct. Is easier to do this than copying the guy we're next to, but that might be necessary. Def check this
	node->ignore_mask = insert_mask(node->tree.treebits, -1, first_guess_pos, !diff_bit_val);
	
	node->key_positions = insert_sorted_pos_to_key_positions(node->key_positions, first_guess_pos, node->tree.meta.size, !diff_bit_val);
	
	node->keys[node->tree.meta.size] = key;
	
	node->tree.meta.size++;
	
	return 0;
}

int query_branch(fusion_node* node, __m512i key) {
	uint16_t first_basemask = extract_bits(&node->tree, key);
	
	int first_guess_pos = search_pos_tree(node, first_basemask);
	
	__m512i first_guess = get_key_from_sorted_pos(node, first_guess_pos);
	
	int diff_bit_pos = first_diff_bit_pos(first_guess, key);
	
	if (diff_bit_pos == -1) { //key is already in there
		return !first_guess_pos; //idk if its negative then let's say you've found the exact key
	}
	
	int mask_pos = diff_bit_to_mask_pos(node, diff_bit_pos);
	int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
	int second_guess_pos = search_partial_pos_tree(node, first_basemask, mask_pos, !diff_bit_val); //fix this!
	return second_guess_pos;
}