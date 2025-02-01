////////////////////////////////
//~ tec: Arena Functions

//- tec: arena creation/destruction

internal Arena *
arena_alloc_(ArenaParams *params)
{
  // tec: round up reserve/commit sizes
  U64 reserve_size = params->reserve_size;
  U64 commit_size = params->commit_size;
  if(params->flags & ArenaFlag_LargePages)
  {
    reserve_size = AlignPow2(reserve_size, os_get_system_info()->large_page_size);
    commit_size  = AlignPow2(commit_size,  os_get_system_info()->large_page_size);
  }
  else
  {
    reserve_size = AlignPow2(reserve_size, os_get_system_info()->page_size);
    commit_size  = AlignPow2(commit_size,  os_get_system_info()->page_size);
  }
  
  // tec: reserve/commit initial block
  void *base = params->optional_backing_buffer;
  if(base == 0)
  {
    if(params->flags & ArenaFlag_LargePages)
    {
      base = os_reserve_large(reserve_size);
      os_commit_large(base, commit_size);
    }
    else
    {
      base = os_reserve(reserve_size);
      os_commit(base, commit_size);
    }
  }
  
  // tec: panic on arena creation failure
#if OS_FEATURE_GRAPHICAL
  if(Unlikely(base == 0))
  {
    os_graphical_message(1, str8_lit("Fatal Allocation Failure"), str8_lit("Unexpected memory allocation failure."));
    os_abort(1);
  }
#endif
  if (base == 0)
  {
    log_error("fatal allocation failure");
  }
  
  // tec: extract arena header & fill
  Arena *arena = (Arena *)base;
  arena->current = arena;
  arena->flags = params->flags;
  arena->cmt_size = (U32)params->commit_size;
  arena->res_size = params->reserve_size;
  arena->base_pos = 0;
  arena->pos = ARENA_HEADER_SIZE;
  arena->cmt = commit_size;
  arena->res = reserve_size;
  AsanPoisonMemoryRegion(base, commit_size);
  AsanUnpoisonMemoryRegion(base, ARENA_HEADER_SIZE);
  return arena;
}

internal void
arena_release(Arena *arena)
{
  for(Arena *n = arena->current, *prev = 0; n != 0; n = prev)
  {
    prev = n->prev;
    os_release(n, n->res);
  }
}

//- tec: arena push/pop core functions

internal void *
arena_push(Arena *arena, U64 size, U64 align)
{
  Arena *current = arena->current;
  U64 pos_pre = AlignPow2(current->pos, align);
  U64 pos_pst = pos_pre + size;
  
  // tec: chain, if needed
  if(current->res < pos_pst && !(arena->flags & ArenaFlag_NoChain))
  {
    U64 res_size = current->res_size;
    U64 cmt_size = current->cmt_size;
    if(size > cmt_size)
    {
      /*
      res_size = size + ARENA_HEADER_SIZE;
      cmt_size = size + ARENA_HEADER_SIZE;
      */
      res_size = AlignPow2(size + ARENA_HEADER_SIZE, os_get_system_info()->page_size);
      cmt_size = Min(res_size, arena_default_commit_size);
    }
    Arena *new_block = arena_alloc(.reserve_size = res_size,
                                   .commit_size = cmt_size,
                                   .flags = current->flags);
    if (!new_block)
    {
      log_error("failed alloc new chain arena");
    }
    new_block->base_pos = current->base_pos + current->res;
    SLLStackPush_N(arena->current, new_block, prev);
    current = new_block;
    pos_pre = AlignPow2(current->pos, align);
    pos_pst = pos_pst + size;
  }
  
  // tec: commit new pages, if needed
  if(current->cmt < pos_pst && !(current->flags & ArenaFlag_LargePages))
  {
    U64 cmt_pst_aligned = AlignPow2(pos_pst, current->cmt_size);
    U64 cmt_pst_clamped = Min(cmt_pst_aligned, current->res);
    
    U64 cmt_size = cmt_pst_clamped - current->cmt;
    
    log_debug("Commit Debug: pos_pst=%llu, cmt_pst_aligned=%llu (adjusted=%llu), current->res=%llu, current->cmt=%llu",pos_pst, cmt_pst_aligned, current->res, current->cmt);
    if (cmt_size > 0)
    {
      if (!os_commit((U8 *)current + current->cmt, cmt_size))
      {
        log_error("os_commit failed to allocate memory");
      }
    }
    else
    {
      //log_error("commit size was 0");
    }
    current->cmt = cmt_pst_clamped;
  }
  
  // tec: push onto current block
  void *result = 0;
  if(current->cmt >= pos_pst)
  {
    result = (U8 *)current+pos_pre;
    current->pos = pos_pst;
    AsanUnpoisonMemoryRegion(result, size);
  }
  
  // tec: panic on failure
#if OS_FEATURE_GRAPHICAL
  if(Unlikely(result == 0))
  {
    os_graphical_message(1, str8_lit("Fatal Allocation Failure"), str8_lit("Unexpected memory allocation failure."));
    os_abort(1);
  }
#endif
  if (result == 0)
  {
    log_error("arena_push() failed: size=%llu, align=%llu, current->pos=%llu, current->res=%llu", size, align, current->pos, current->res);
  }
  
  
  return result;
}

internal U64
arena_pos(Arena *arena)
{
  Arena *current = arena->current;
  U64 pos = current->base_pos + current->pos;
  return pos;
}

internal void
arena_pop_to(Arena *arena, U64 pos)
{
  U64 big_pos = ClampBot(ARENA_HEADER_SIZE, pos);
  Arena *current = arena->current;
  for(Arena *prev = 0; current->base_pos >= big_pos; current = prev)
  {
    prev = current->prev;
    os_release(current, current->res);
  }
  arena->current = current;
  U64 new_pos = big_pos - current->base_pos;
  AssertAlways(new_pos <= current->pos);
  AsanPoisonMemoryRegion((U8*)current + new_pos, (current->pos - new_pos));
  current->pos = new_pos;
}

//- tec: arena push/pop helpers

internal void
arena_clear(Arena *arena)
{
  arena_pop_to(arena, 0);
}

internal void
arena_pop(Arena *arena, U64 amt)
{
  U64 pos_old = arena_pos(arena);
  U64 pos_new = pos_old;
  if(amt < pos_old)
  {
    pos_new = pos_old - amt;
  }
  arena_pop_to(arena, pos_new);
}

//- tec: temporary arena scopes

internal Temp
temp_begin(Arena *arena)
{
  U64 pos = arena_pos(arena);
  Temp temp = {arena, pos};
  return temp;
}

internal void
temp_end(Temp temp)
{
  arena_pop_to(temp.arena, temp.pos);
}