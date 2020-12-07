#include "fn_index.h"

static struct PgIndex {
    void    		* address;
	struct PgIndex 	* next;
    u32     		page_num;
	u32				indegree;
} *pg_index;

static size_t fn_count;
static struct FnIndex {
	u32 page_num;
    u32 line_num;
	u32 idx;
} *fn_index;

static int fidx_cmp(const void *ptr1, const void *ptr2) {
	const struct FnIndex *f1 = ptr1;
	const struct FnIndex *f2 = ptr2;
	if (f1->page_num != f2->page_num) return f1->page_num - f2->page_num;
    return f1->line_num - f2->line_num;
}

M3Result build_pg_index(IM3Runtime runtime) {
    u32 pg_count = runtime->numCodePages;
    pg_index = malloc(sizeof(*pg_index) * pg_count);
    u32 i = 0;
    for (IM3CodePage pg = runtime->pagesFull; pg; pg = pg->info.next) {
        pg_index[i].page_num = i;
		pg_index[i].address = pg->code;
		pg_index[i].indegree = 0;
		pg_index[i].next = NULL;
		i++;
    
}

// works for single module only
void build_fn_index(IM3Runtime runtime) {
	fn_count = runtime->modules->numFunctions;
	fn_index = malloc(sizeof(*fn_index) * fn_count);
	for (size_t i = 0; i < fn_count; i++) {
		fn_index[i].ptr = runtime->modules->functions[i].compiled;
		fn_index[i].idx = i;
	}
	
	qsort(fn_index, fn_count, sizeof(*fn_index), fidx_cmp);
}

void find_fn(pc_t pc, u64 *fn_id, u64 *offset) {
	struct FnIndex *fn = &fn_index[0];
	i32 lo = 0, hi = fn_count;
	while (lo < hi) {
		i32 mid = lo + (hi - lo)/2;
		int cmp = fidx_cmp(&fn_index[mid], &pc);
		if (cmp == 0) {
			fn = &fn_index[mid];
			break;
		} else if (cmp < 0) {
			lo = mid+1;
			fn = &fn_index[mid];
		} else {
			hi = mid;
		}
	}

	*fn_id = fn->idx;
	*offset = (u64)pc - (u64)fn->ptr;
}