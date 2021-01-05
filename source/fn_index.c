#include "fn_index.h"

static inline int pptrcmp(const void *pp1, const void *pp2) {
	const void *p1 = *(const void**)pp1;
	const void *p2 = *(const void**)pp2;
	if (p1 < p2) return -1;
	else if (p1 == p2) return 0;
	return 1;
}

void *bsearch_le(void *key,
				 size_t count,
				 size_t size, 
				 char (*base)[size],
				 int (*compar)(const void *, const void*)) 
{
	void *result = NULL;
	size_t lo = 0, hi = count;
	while (lo < hi) {
		i64 mid = lo + (hi - lo)/2;
		int cmp = compar(base[mid], key);
		if (cmp == 0) {
			result = &base[mid];
			break;
		} else if (cmp < 0) {
			lo = mid+1;
			result = &base[mid];
		} else {
			hi = mid;
		}
	}
	return result;
}

void *bsearch_ge(void *key,
				 size_t count,
				 size_t size, 
				 char (*base)[size],
				 int (*compar)(const void *, const void*)) 
{
	void *result = NULL;
	size_t lo = 0, hi = count;
	while (lo < hi) {
		i64 mid = lo + (hi - lo)/2;
		int cmp = compar(base[mid], key);
		if (cmp == 0) {
			result = &base[mid];
			break;
		} else if (cmp < 0) {
			lo = mid+1;
		} else {
			hi = mid;
			result = &base[mid];
		}
	}
	return result;
}

// works for single module only
void build_fn_index(IM3Module module) {
	qsort(module->fnTable, module->fnTableSize, sizeof(MGFnTableEntry), pptrcmp);
	qsort(module->bridges, module->bridgeCount, sizeof(pc_t), pptrcmp);
}


/** find the function and offset from start for a pc value, traversing any
 *  bridges and subtracting the bridge lengths
 * 
 *  @param fn_id: pointer which would be filled with function id
 *  @param offset: pointer which would be filled with pc's offset inside the
 *         function. The offset is in steps of sizeof(code_t)
 */
void find_fn_and_offset(IM3Module module, pc_t pc, u64 *fn_id, u64 *offset) {

	MGFnTableEntry *fn = bsearch_le(
		&pc, 
		module->fnTableSize, 
		sizeof(MGFnTableEntry), 
		module->fnTable, 
		pptrcmp);
	
	u64 tmp_off = 0;
	pc_t head = module->functions[fn->idx].compiled;
	while (1) {
		pc_t *next_bridge = bsearch_ge(&head, module->bridgeCount, sizeof(pc_t), 
									   module->bridges, pptrcmp);
		if (head > pc || (next_bridge && *next_bridge < pc)) {
			tmp_off += *next_bridge - head;
			head = (*next_bridge)[1]; // head moves to bridge target
		} else {
			tmp_off += pc - head;
			break;
		}
	}

	*fn_id = fn->idx;
	*offset = tmp_off;
}

/** retrieve pc corresponding to given offset from in the function given by fn_id
 */ 
pc_t goto_fn_offset(IM3Module module, u64 fn_id, u64 offset)
{
	u64 o2 = offset;
	pc_t pc = module->functions[fn_id].compiled;
	while (1) {
		pc_t *next_bridge = bsearch_ge(&pc, module->bridgeCount, sizeof(pc_t), 
									   module->bridges, pptrcmp);
		if (next_bridge && *next_bridge - pc < offset) {
			offset -= *next_bridge - pc;
			pc = (*next_bridge)[1];
		} else {
			return pc + offset;
		}
	}
}