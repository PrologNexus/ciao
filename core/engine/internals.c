/*
 *  internals.c
 *
 *  Copyright (C) 1996-2002 UPM-CLIP
 *  Copyright (C) 2020 The Ciao Development Team
 */

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <ciao/eng.h>
#include <ciao/eng_registry.h>
#include <ciao/basiccontrol.h>
#include <ciao/os_signal.h>

#include <ciao/stream_basic.h>
#include <ciao/io_basic.h>
#include <ciao/eng_start.h>
#include <ciao/internals.h>
#include <ciao/dynamic_rt.h>
#include <ciao/rt_exp.h>
#include <ciao/eng_interrupt.h>

#include <ciao/eng_bignum.h>
#include <ciao/eng_gc.h>
#include <ciao/timing.h>
#include <ciao/eng_profile.h>

CBOOL__PROTO(stack_shift_usage);
CBOOL__PROTO(termheap_usage);
CBOOL__PROTO(envstack_usage);
CBOOL__PROTO(choice_usage);
CBOOL__PROTO(trail_usage);

/* --------------------------------------------------------------------------- */
/* Memory usage and statistics */

/* termheap_usage: [sizeof_used_space, sizeof_free_space] */
CBOOL__PROTO(termheap_usage) {
  intmach_t used, free;
  tagged_t x;
  
  used = HeapCharDifference(Heap_Start,w->heap_top);
  free = HeapCharDifference(w->heap_top,Heap_End);
  MakeLST(x,IntmachToTagged(free),atom_nil);
  MakeLST(x,IntmachToTagged(used),x);
  CBOOL__LASTUNIFY(X(0),x);
}

/* envstack_usage: [sizeof_used_space, sizeof_free_space] */
CBOOL__PROTO(envstack_usage) {
  intmach_t used, free;
  tagged_t x;
  frame_t *newa;

  GetFrameTop(newa,w->choice,G->frame);
  used = StackCharDifference(Stack_Start,newa);
  free = StackCharDifference(newa,Stack_End);
  MakeLST(x,IntmachToTagged(free),atom_nil);
  MakeLST(x,IntmachToTagged(used),x);
  CBOOL__LASTUNIFY(X(0),x);
}

/* choice_usage: [sizeof_used_space, sizeof_free_space] */
CBOOL__PROTO(choice_usage) {
  intmach_t used, free;
  tagged_t x;
  
  used = ChoiceCharDifference(Choice_Start,w->choice);
  free = ChoiceCharDifference(w->choice,w->trail_top)/2;
  MakeLST(x,IntmachToTagged(free),atom_nil);
  MakeLST(x,IntmachToTagged(used),x);
  CBOOL__LASTUNIFY(X(0),x);
}

/* trail_usage: [sizeof_used_space, sizeof_free_space] */
CBOOL__PROTO(trail_usage) {
  intmach_t used, free;
  tagged_t x;
  
  used = TrailCharDifference(Trail_Start,w->trail_top);
  free = TrailCharDifference(w->trail_top,w->choice)/2;
  MakeLST(x,IntmachToTagged(free),atom_nil);
  MakeLST(x,IntmachToTagged(used),x);
  CBOOL__LASTUNIFY(X(0),x);
}

/* stack_shift_usage: [global shifts,local+control/trail shifts,time spent] */
CBOOL__PROTO(stack_shift_usage) {
  tagged_t x;
  inttime_t time = (ciao_stats.ss_tick*1000)/RunClockFreq(ciao_stats);
  
  MakeLST(x,IntmachToTagged(time),atom_nil);
  time = ciao_stats.ss_local+ciao_stats.ss_control;
  MakeLST(x,IntmachToTagged(time),x);
  time = ciao_stats.ss_global;
  MakeLST(x,IntmachToTagged(time),x);
  CBOOL__LASTUNIFY(X(0),x);
}

/* ------------------------------------------------------------------------- */
/* Garbage collection parameters */

CBOOL__PROTO(gc_usage) {
  flt64_t t;
  tagged_t x;

  t = (flt64_t)ciao_stats.gc_tick*1000/RunClockFreq(ciao_stats);
  MakeLST(x,BoxFloat(t),atom_nil);
  t = ciao_stats.gc_acc;
  MakeLST(x,IntmachToTagged((intmach_t)t),x);
  t = ciao_stats.gc_count;
  MakeLST(x,IntmachToTagged((intmach_t)t),x);
  CBOOL__LASTUNIFY(x,X(0));
}

tagged_t gcmode_to_term(bool_t gcmode) {
  if (gcmode == TRUE) {
    return atom_on;
  } else {
    return atom_off;
  }
}

bool_t gcmode_from_term(tagged_t mode) {
  if (mode == atom_on) {
    return TRUE;
  } else {
    return FALSE;
  }
}

tagged_t gctrace_to_term(intmach_t gctrace) {
  if (gctrace == GCTRACE__OFF) {
    return atom_off;
  } else if (gctrace == GCTRACE__TERSE) {
    return atom_terse;
  } else {
    return atom_verbose;
  }
}

intmach_t gctrace_from_term(tagged_t trace) {
  if (trace == atom_off) {
    return GCTRACE__OFF;
  } else if (trace == atom_terse) {
    return GCTRACE__TERSE;
  } else {
    return GCTRACE__VERBOSE;
  }
}

CBOOL__PROTO(gc_mode) {
  CBOOL__UnifyCons(gcmode_to_term(current_gcmode),X(0));
  DEREF(X(1), X(1));
  current_gcmode = gcmode_from_term(X(1));
  CBOOL__PROCEED;
}

CBOOL__PROTO(gc_trace) {
  CBOOL__UnifyCons(gctrace_to_term(current_gctrace),X(0));
  DEREF(X(1), X(1));
  current_gctrace = gctrace_from_term(X(1));
  CBOOL__PROCEED;
}

CBOOL__PROTO(gc_margin) {
  CBOOL__UnifyCons(MakeSmall(current_gcmargin),X(0));
  DEREF(X(1), X(1));
  current_gcmargin = GetSmall(X(1));
  CBOOL__PROCEED;
}

/*-------------------------------------------------------*/

void add_definition(sw_on_key_t **swp,
                    sw_on_key_node_t *node,
                    tagged_t key,
                    definition_t *def)
{
  node->key=key;
  node->value.def=def;
  if (((*swp)->count+=1)<<1 > SwitchSize(*swp))
    expand_sw_on_key(swp,NULL,FALSE);
}

definition_t *insert_definition(sw_on_key_t **swp,
                                tagged_t tagpname,
                                int arity,
                                bool_t insertp)
{
  sw_on_key_node_t *keyval;
  definition_t *value = NULL;
  tagged_t key=SetArity(tagpname,arity);

  /* Lock here -- we do not want two different workers to add predicates
     concurrently. */

  Wait_Acquire_slock(prolog_predicates_l);
  keyval = (sw_on_key_node_t *)incore_gethash((*swp),key);

  if (keyval->key)                                    /* Already existent */
    value = keyval->value.def;
  else if (insertp){                                      /* New predicate */
    value=new_functor(tagpname, arity);
    add_definition(swp, keyval, key, value);
  }

  Release_slock(prolog_predicates_l);

  return value;
}

/*------------------------------------------------------------*/

definition_t *find_definition(sw_on_key_t **swp,
                              tagged_t term, tagged_t **argl,
                              bool_t insertp)
{
  int arity;

  if (TaggedIsStructure(term)) {
    tagged_t f = TaggedToHeadfunctor(term);

    *argl = TaggedToArg(term,1);
    term = SetArity(f,0);
    arity = Arity(f);
  } else
    if (TaggedIsLST(term)) {
      *argl = TagpPtr(LST,term);
      term = atom_list;
      arity = 2;
    }
    else
      arity = 0;

  return insert_definition(swp,term,arity,insertp);
}

/* --------------------------------------------------------------------------- */

static definition_t *parse_1_definition(tagged_t tagname, tagged_t tagarity);

/* Enter here with a *definition name* as generated by the compiler. */
definition_t *parse_definition(tagged_t complex)
{
  tagged_t a,b;

  if (TaggedIsSTR(complex) && (TaggedToHeadfunctor(complex)==functor_slash)) {
    DerefArg(a,complex,1);
    DerefArg(b,complex,2);
    return parse_1_definition(a,b);
  }
  else return NULL;
}

static definition_t **find_subdef_chain(definition_t *f, intmach_t clause_no)
{
  incore_info_t *d = f->code.incoreinfo;
  emul_info_t *ep;

  if (clause_no != 0 && clause_no <= d->clauses_tail->number)
    for (ep = d->clauses.ptr; --clause_no; ep = ep->next.ptr)
      ;
  else {
    /* TODO: (JFMC) assumes that &(d->clauses_tail->next) == d->clauses_tail */
    ep = (emul_info_t *)d->clauses_tail;
    if (!IS_CLAUSE_TAIL(ep) || ep->next.ptr == NULL) {
      ALLOC_CLAUSE_TAIL(ep);
      ep->next.number = d->clauses_tail->number + 1;
      d->clauses_tail->ptr = ep;
      d->clauses_tail = &ep->next;
    }
  }
  return &ep->subdefs;
}

static definition_t *parse_1_definition(tagged_t tagname, tagged_t tagarity)
{
  int arity;

  if (!TaggedIsSmall(tagarity))
    return NULL;
  arity = GetSmall(tagarity);
  if (TaggedIsSTR(tagname) && (TaggedToHeadfunctor(tagname)==functor_minus))
    /* "internal" predicate */
    {
      definition_t *f, *f1, **pf;
      tagged_t tmp;
      intmach_t i;
      intmach_t subdef_no, clause_no;

      DerefArg(tmp,tagname,2);
      subdef_no = GetSmall(tmp);
      DerefArg(tagname,tagname,1);

      DerefArg(tmp,tagname,2);
      clause_no = GetSmall(tmp);
      DerefArg(tagname,tagname,1);

      f = parse_definition(tagname);
      if (f==NULL)
        return NULL;
      i = f->predtyp;
      if (i > ENTER_FASTCODE_INDEXED)
        return NULL;
      pf = find_subdef_chain(f, clause_no);

      if (!(*pf))
        f = *pf = new_functor((tagged_t)f|3, arity);
      else
        {
          for (i=1, f1 = *pf;
               !(f1->printname&2);
               i++, f1 = (definition_t *)TaggedToPointer(f1->printname))
            if (i==subdef_no) break;
        
          if (i==subdef_no) return f1;
          f1->printname = (tagged_t)(f=new_functor(f1->printname, arity))|1;
        }
      return f;
    }

  if (TaggedIsATM(tagname)) 
    return insert_definition(predicates_location,tagname,arity,TRUE);
  else return NULL;
}

/*-------------------------------------------------------*/

/* Inserts the definition of module. */

module_t *new_module(tagged_t mod_atm)
{
  module_t *mod;

  mod = checkalloc_TYPE(module_t);
  mod->printname = mod_atm;
  mod->properties.is_static = FALSE;
  return mod;
}

void add_module(sw_on_key_t **swp,
                sw_on_key_node_t *node,
                tagged_t key,
                module_t *mod)
{
  node->key=key;
  node->value.mod=mod;
  if (((*swp)->count+=1)<<1 > SwitchSize(*swp))
    expand_sw_on_key(swp,NULL,FALSE);
}

module_t *insert_module(sw_on_key_t **swp,
                        tagged_t mod_atm,
                        bool_t insertp)
{
  sw_on_key_node_t *keyval;
  module_t *value = NULL;
  tagged_t key = mod_atm;

  /* Lock here -- we do not want two different workers to add modules
     concurrently. */

  Wait_Acquire_slock(prolog_modules_l);
  keyval = (sw_on_key_node_t *)incore_gethash((*swp),key);

  if (keyval->key)                                    /* Already existent */
    value = keyval->value.mod;
  else if (insertp){                                      /* New module */
    value = new_module(mod_atm);
    add_module(swp, keyval, key, value);
  }

  Release_slock(prolog_modules_l);

  return value;
}

/*-------------------------------------------------------*/

/* Inserts the definition of a predicate, either asserted, compiled,
   consulted, or qloaded. */

definition_t *new_functor(tagged_t tagpname, int arity)
{
  definition_t *func;
  intmach_t i;

  /* How to get the printable name (i.e., accessing to the atom part):

  if ((tagpname & 3) == 0)
    printf("New predicate head %s, arity %d\n", GetString(tagpname), arity);
    */

  func = checkalloc_TYPE(definition_t);
  /* Initialize all fields to 0 */
  for (i=0; i<sizeof(definition_t); i++) {
    ((char *)func)[i] = 0;
  }
  
  func->printname = tagpname;
  func->arity = arity;
  SetEnterInstr(func, ENTER_UNDEFINED);
  func->code.undinfo = NULL;
  return func;
}

/*-----------------------------------------------------------*/

/* segfault patch -- jf */
CVOID__PROTO(trail_push_check, tagged_t x) {
  tagged_t *tr;
  tr = w->trail_top;

  TrailPush(tr,x);
  w->trail_top = tr;
  if (ChoiceYounger(w->choice,TrailOffset(tr,CHOICEPAD)))
    choice_overflow(Arg,2*CHOICEPAD*sizeof(tagged_t),TRUE);
}

/* --------------------------------------------------------------------------- */

/* Create a new copy on the heap of the blob pointed by ptr */
CFUN__PROTO(make_blob, tagged_t, tagged_t *ptr) {
  tagged_t *h = w->heap_top;
  tagged_t f = *ptr;
  intmach_t ar = LargeArity(f);
  intmach_t i;

  for (i=0; i<ar; i++) {
    *h++ = *ptr++;
  }
  *h++ = f;

  w->heap_top = h;
  return Tagp(STR, h-ar-1);
}

#if BC_SCALE == 2
/* Create a blob on the heap from bytecode.

   If the object is a bignum, we use the canonized length and convert
   it to a NUM if possible (this may happend with BC_SCALE == 2).
 */
CFUN__PROTO(bc_make_blob, tagged_t, tagged_t *ptr) {
  intmach_t ar;
  tagged_t f = ptr[0];

  if (FunctorIsFloat(f)) { /* float */
    // fprintf(stderr, "BC_MakeBlob->float\n");
    ar = LargeArity(f);
  } else { /* bignum */
    intmach_t len = bn_canonized_length((bignum_t *)ptr);
    ar = len + 1;
    /* TODO: factorize */
    if (ar==2 && IsInSmiValRange((intmach_t)ptr[1])) {
      // fprintf(stderr, "BC_MakeBlob->small\n");
      return MakeSmall(ptr[1]);
    }
    // fprintf(stderr, "BC_MakeBlob->bignum\n");
    f = BlobFunctorBignum(len);
  }

  /* Copy blob into the heap */
  tagged_t *h = w->heap_top;
  *h++ = f; ptr++;
  for (intmach_t i=1; i<ar; i++) *h++ = *ptr++;
  *h++ = f;
  w->heap_top = h;
  return Tagp(STR, h-ar-1);
}

/* (assume t deref) */
CBOOL__PROTO(bc_eq_blob, tagged_t t, tagged_t *ptr) {
  intmach_t ar;
  tagged_t f = ptr[0];

  if (FunctorIsFloat(f)) { /* float */
    // fprintf(stderr, "eq_blob->float\n");
    ar = LargeArity(f);
  } else { /* bignum */
    intmach_t len = bn_canonized_length((bignum_t *)ptr);
    ar = len + 1;
    /* TODO: factorize */
    if (ar==2 && IsInSmiValRange((intmach_t)ptr[1])) {
      // fprintf(stderr, "eq_blob->small\n");
      return t == MakeSmall(ptr[1]);
    }
    // fprintf(stderr, "eq_blob->bignum\n");
  }

  /* Compare blob from the heap */
  if (!TaggedIsSTR(t)) return FALSE;
  for (intmach_t i=ar; i>0; i--) {
    if (ptr[i-1] != *TaggedToArg(t,i-1)) return FALSE;
  }
  return TRUE;
}
#endif

CFUN__PROTO(make_integer, tagged_t, intmach_t i) {
  tagged_t *h = w->heap_top;

  HeapPush(h, MakeFunctorFix);
  HeapPush(h, (tagged_t)i);
  HeapPush(h, MakeFunctorFix);
  w->heap_top = h;
  return Tagp(STR, h-3);
}

CFUN__PROTO(make_float, tagged_t, flt64_t i) {
  tagged_t *h;
  h = w->heap_top;
  HeapPush(h, BlobFunctorFlt64);
  HeapPushFlt64(h, i);
#if (!defined(OPTIM_COMP)) && LOG2_bignum_size==6 && BC_SCALE==2
  HeapPush(h, 0); /* TODO: avoid dummy word for BC_SCALE==2? (fix length?) */
#endif
  HeapPush(h, BlobFunctorFlt64);
  w->heap_top = h;
  return Tagp(STR, h-4);
}

/* Pre: !IsSmall(t) (small int's taken care of by TaggedToIntmach()) */
intmach_t get_integer(tagged_t t) {
  if (LargeIsFloat(t)) {
    return blob_to_flt64(t);
  } else {
    return (intmach_t)*TaggedToArg(t,1);
  }
}

#define TaggedToArg(X,N) HeapOffset(TagpPtr(STR,X),N)
#define TaggedToHeadfunctor(X) (*TagpPtr(STR,X))
#define BignumRawLength(b) (*(functor_t *)(b))
#define BignumLength(b) FunctorBignumValue(BignumRawLength(b))

#if 0
  SwBlob(t, functor, { /* STR(blob(float)) */
    flt64_t f;
    UnboxFlt64(HeapCharOffset(TagpPtr(STR, t), sizeof(functor_t)), f);
    return f;
  }, { /* STR(blob(bignum)) */
    return bn_to_float((bignum_t *)TagpPtr(STR, t));
  });
#endif

flt64_t bn_to_float(bignum_t *bn);

/* Pre: !IsSmall(t) (small int's taken care of by TaggedToFloat()) */
flt64_t blob_to_flt64(tagged_t t) {
  if (!LargeIsFloat(t)) {
    return bn_to_float(TagpPtr(STR, t));
  } else { /* LargeIsFloat(t) */
    union {
      flt64_t i;
      tagged_t p[sizeof(flt64_t)/sizeof(tagged_t)];
    } u;
#if LOG2_bignum_size == 5
    u.p[0] = *TaggedToArg(t,1);
    u.p[1] = *TaggedToArg(t,2);
#elif LOG2_bignum_size == 6
    u.p[0] = *TaggedToArg(t,1);
#endif
    return u.i;
  }
}

/* --------------------------------------------------------------------------- */
/* Create a most general term for a given functor or small int. */

CFUN__PROTO(make_structure, tagged_t,
            tagged_t functor)
{
  intmach_t ar = Arity(functor);
  tagged_t *h = w->heap_top;

  if (ar==0 || !TaggedIsATM(functor))
    return functor;
  else if (functor==functor_lst) {
    ConstrHVA(h);
    ConstrHVA(h);
    w->heap_top = h;
    return Tagp(LST,HeapOffset(h,-2));
  } else {
    HeapPush(h,functor);
    do {
      ConstrHVA(h);
    } while (--ar);
    w->heap_top = h;

    return Tagp(STR,h-Arity(functor)-1);
  }
}

/* --------------------------------------------------------------------------- */
/* Support for the incremental clause compiler. */

static void set_nondet(try_node_t *t,
                       incore_info_t *def,
                       bool_t first);
static void incore_insert(try_node_t  **t0, 
                          int effar,
                          emul_info_t *ref,
                          incore_info_t *def);
static void incore_puthash(sw_on_key_t **psw, 
                           int effar, 
                           emul_info_t *current, 
                           incore_info_t *def,
                           tagged_t k);
static try_node_t *incore_copy(try_node_t *from);
static void free_try(try_node_t **t);
static void free_sw_on_key(sw_on_key_t **sw);
static void free_emulinfo(emul_info_t *cl);
static void free_incoreinfo(incore_info_t **p);
static CVOID__PROTO(make_undefined, definition_t *f);
static void free_info(enter_instr_t enter_instr, char *info);
void init_interpreted(definition_t *f);


#define ISNOTRY(T)              (((T)==NULL) || ((T)==fail_alt))

/* Indexing for the incore compiler. */

/* Patch bytecode information related to the last clause inserted */

static void set_nondet(try_node_t *t,
                       incore_info_t *def,
                       bool_t first)
{
  uintmach_t i;
  emul_info_t *cl;

  /* Check if we can use the last cached clause and number to insert */

#if defined(CACHE_INCREMENTAL_CLAUSE_INSERTION)

  /* If this is activated, patching is sped up by caching the last
     insertion peformed, and using the cache not to advance in the
     chain of clauses from the beginning.  Inserting always at the end
     is simply not possible because the clause numbers to be accessed
     do not come ordered. The cache is used only if we are inserting
     farther than the last clause inserted.

     We should check that the clause numbers have not changed ---
     i.e., that intermediate records are not erased --- as that would
     invalidate our count.
  */

  if (t->number >= def->last_inserted_num){/* farther than last insertion */
    cl = def->last_inserted_clause;
    i  = t->number - def->last_inserted_num;
  } else {
    i  = t->number - 1;
    cl = def->clauses.ptr;
  }

  for( ; i; i--)/* Skip until end of chain --- it is not NULL terminated! */
    cl = cl->next.ptr;

  def->last_inserted_clause = cl;
  def->last_inserted_num    = t->number;
#else
  for (i=t->number, cl=def->clauses.ptr; --i;) /* 1-based numbers */
    cl = cl->next.ptr;
#endif

 /* Patch previous emul_p "fail_alt" code */
  t->emul_p = BCoff(cl->emulcode, FTYPE_size(f_i));
  if (first) { /* maintain emul_p2 optimization */
    t->emul_p2 = BCoff(t->emul_p, p2_offset(BCOp(t->emul_p, FTYPE_ctype(f_o), 0)));
  }
}


static void incore_insert(try_node_t **t0,
                          int effar,
                          emul_info_t *ref,
                          incore_info_t *def)
{
  try_node_t **t1 = t0;
  try_node_t *t;

  /* Init the try_node to insert. */
  t = checkalloc_TYPE(try_node_t);
  t->arity = effar;
  /* Last "next" is num. of clauses: we are inserting at the end of the chain */
  t->number = ref->next.number;
  t->emul_p = BCoff(ref->emulcode, BCOp(ref->emulcode, FTYPE_ctype(f_i), 0)); /* initial p: det case */
#if defined(GAUGE)
  t->entry_counter = ref->counters;
#endif
  t->next = NULL;

  if (ISNOTRY(*t1)) {
    t->emul_p2 = BCoff(t->emul_p, p2_offset(BCOp(t->emul_p, FTYPE_ctype(f_o), 0)));
  } else {
    do {
      if ((*t1)->next==NULL)
        set_nondet(*t1,def,t0==t1);
      t1 = &(*t1)->next;
    } while (!ISNOTRY(*t1));
  }
  (*t1) = t;
}

static try_node_t *incore_copy(try_node_t *from)
{
  try_node_t *tcopy = fail_alt;
  try_node_t **to = &tcopy;

  for (; !ISNOTRY(from); from=from->next) {
    (*to) = checkalloc_TYPE(try_node_t);
    (*to)->arity = from->arity;
    (*to)->number = from->number;
    (*to)->emul_p = from->emul_p;
    (*to)->emul_p2 = from->emul_p2;
    (*to)->next = NULL;
#if defined(GAUGE)
    (*to)->entry_counter = from->entry_counter;
#endif
    to = &(*to)->next;
  }

  return tcopy;
}

/* get location of try chain for a key */
sw_on_key_node_t *incore_gethash(sw_on_key_t *sw, tagged_t key) {
  sw_on_key_node_t *hnode;
  intmach_t i;
  tagged_t t0;

  for (i=0, t0=key & sw->mask;
       ;
       i+=sizeof(sw_on_key_node_t), t0=(t0+i) & sw->mask) {
    hnode = SW_ON_KEY_NODE_FROM_OFFSET(sw, t0);
    if (hnode->key==key || !hnode->key)
      return hnode;
  }
}

sw_on_key_t *new_switch_on_key(intmach_t size,
                               try_node_t *otherwise)
{
  intmach_t i;
  sw_on_key_t *sw;

  sw = checkalloc_FLEXIBLE(sw_on_key_t, sw_on_key_node_t, size);

  sw->mask = SizeToMask(size);
  sw->count = 0;
#if defined(ATOMGC)
  sw->next_index = 0;
#endif
  for (i=0; i<size; i++)
    sw->node[i].key = 0,
    sw->node[i].value.try_chain = otherwise;
  return sw;
}

void expand_sw_on_key(sw_on_key_t **psw,
                      try_node_t *otherwise,
                      bool_t deletep)
{
  sw_on_key_node_t *h1, *h2;
  intmach_t size = SwitchSize(*psw);
  sw_on_key_t *newsw = new_switch_on_key(size<<1,otherwise);
  intmach_t j;

  for (j=size-1; j>=0; --j) {
    h1 = &(*psw)->node[j];
    if (h1->key) {
      newsw->count++;
      h2 = incore_gethash(newsw,h1->key);
      h2->key = h1->key;
      h2->value.try_chain = h1->value.try_chain;
    }
  }

  if (deletep) {
    checkdealloc_FLEXIBLE(sw_on_key_t,
                          sw_on_key_node_t,
                          size,
                          *psw);
  } else {
    leave_to_gc(TABLE, (char *)(*psw));
  }

  (*psw) = newsw;
}

/* I still could not make the "pointer to the last try_node" work with the
has table try nodes; I am passing a NULL pointer which is checked by
incore_insert() and not used.  */

static void incore_puthash(sw_on_key_t **psw,
                           int effar,
                           emul_info_t *current,
                           incore_info_t *def,
                           tagged_t k)
{
  intmach_t i;
  sw_on_key_node_t *h1;
  try_node_t *otherwise = NULL;
  intmach_t size = SwitchSize(*psw);

  if (k==ERRORTAG){             /* add an alt. to default and to every key */
    for (i=0; i<size; i++) {
      h1 = &(*psw)->node[i];
      if (h1->key)
        incore_insert(&h1->value.try_chain,effar,current,def);
      else if (!otherwise){
        incore_insert(&h1->value.try_chain,effar,current,def);
        otherwise = h1->value.try_chain;
      } else
        h1->value.try_chain = otherwise;
    }
  } else {
    h1 = incore_gethash(*psw,k);
    if (!h1->key) {
      h1->key = k;
      h1->value.try_chain = incore_copy(otherwise=h1->value.try_chain);
      incore_insert(&h1->value.try_chain,effar,current,def);
      if (((*psw)->count+=1)<<1 > size)
        expand_sw_on_key(psw,otherwise,TRUE);
    } else incore_insert(&h1->value.try_chain,effar,current,def);
  }
}

static void free_try(try_node_t **t)
{
  try_node_t *t1, *t2;

  for (t1=(*t); !ISNOTRY(t1); t1=t2) {
    t2=t1->next;
    checkdealloc_TYPE(try_node_t, t1);
  }
  (*t)=NULL;
}


static void free_sw_on_key(sw_on_key_t **sw)
{
  sw_on_key_node_t *h1;
  intmach_t i;
  intmach_t size = SwitchSize(*sw);
  bool_t otherwise = FALSE;

  for (i=0; i<size; i++) {
    h1 = &(*sw)->node[i];
    if (h1->key || !otherwise)
      free_try(&h1->value.try_chain);
    if (!h1->key)
      otherwise = TRUE;
  }

  checkdealloc_FLEXIBLE(sw_on_key_t,
                        sw_on_key_node_t,
                        size,
                        *sw);
  (*sw)=NULL;
}

static void free_emulinfo(emul_info_t *cl)
{
  definition_t *def, *sibling;

  for (def=cl->subdefs; def!=NULL; def=sibling) {
    sibling = DEF_SIBLING(def);
    free_info(def->predtyp, (char *)def->code.intinfo);
    checkdealloc_TYPE(definition_t, def);
  }
  checkdealloc_FLEXIBLE_S(emul_info_t, objsize, cl);
}

static void free_incoreinfo(incore_info_t **p)
{
  emul_info_t *stop = (*p)->clauses_tail->ptr;
  emul_info_t *cl, *cl1;

  for (cl=(*p)->clauses.ptr; cl!=stop; cl=cl1) {
    cl1 = cl->next.ptr;
    free_emulinfo(cl);
  }
  checkdealloc_TYPE(incore_info_t, *p);
  (*p) = NULL;
}

/* Those have to do with garbage collecting of the abolished predicates.
   Should be made by only one worker?  Otherwise, access should be locked
   when doing this GC --- which means every predicate access should be
   locked! */

static intmach_t gcdef_count=0;             /* Shared, no locked */
static intmach_t gcdef_limit=0;             /* Shared, no locked */
typedef struct gcdef_ gcdef_t;
struct gcdef_ {                 /* Shared, no locked */
  enter_instr_t enter_instr;
  char *info;
};
static gcdef_t *gcdef_bin;

void leave_to_gc(enter_instr_t type, char *info) {
  intmach_t size;

  if (gcdef_limit==0) {
    size = 2;
    gcdef_limit = 2;
    gcdef_bin = checkalloc_ARRAY(gcdef_t, size);
  } else if (gcdef_count==gcdef_limit) {
    size = gcdef_count;
    gcdef_limit *= 2;
    gcdef_bin = checkrealloc_ARRAY(gcdef_t,
                                   size,
                                   size*2,
                                   gcdef_bin);
  }

  gcdef_bin[gcdef_count].enter_instr = type;
  gcdef_bin[gcdef_count].info = info;
  gcdef_count++;
}

CVOID__PROTO(erase_interpreted, definition_t *f);

static CVOID__PROTO(make_undefined, definition_t *f) {
  /*Wait_Acquire_slock(prolog_predicates_l);*/
  leave_to_gc(f->predtyp, (char *)f->code.intinfo);
  if (f->predtyp==ENTER_INTERPRETED) {
    CVOID__CALL(erase_interpreted, f);
  }

#if defined(ABSMACH_OPT__profile_calls)
  f->number_of_calls = 0;
  f->time_spent = 0;
#endif

  /*f->properties.public = 0;*/
  f->properties.wait = 0;
  f->properties.multifile = 0;
  f->properties.dynamic = 0;
#if defined(USE_THREADS)
  f->properties.concurrent = 0;
#endif
  SetEnterInstr(f,ENTER_UNDEFINED);

  /*Release_slock(prolog_predicates_l);*/
}

/* Really get rid of abolished predicate. */
CBOOL__PROTO(empty_gcdef_bin)
{
  gcdef_t *g;
  intmach_t current_mem = total_mem_count;

  while (gcdef_count>0)  {
    g = &gcdef_bin[--gcdef_count];
    free_info(g->enter_instr, g->info);
  }
  INC_MEM_PROG(total_mem_count - current_mem);

  return TRUE;
}

void relocate_gcdef_clocks(instance_clock_t *clocks)
{
  intmach_t i;

  for (i=0; i<gcdef_count; i++)
    if (gcdef_bin[i].enter_instr==ENTER_INTERPRETED)
      relocate_clocks(((int_info_t *)gcdef_bin[i].info)->first, clocks);
}

static void free_info(enter_instr_t enter_instr, char *info)
{
  switch(enter_instr)
    {
    case ENTER_COMPACTCODE_INDEXED:
    case ENTER_PROFILEDCODE_INDEXED:
      free_try(&((incore_info_t *)info)->lstcase);
      free_sw_on_key(&((incore_info_t *)info)->othercase);
    case ENTER_COMPACTCODE:
    case ENTER_PROFILEDCODE:
      free_try(&((incore_info_t *)info)->varcase);
      free_incoreinfo((incore_info_t **)(&info));
      break;
    case ENTER_INTERPRETED:
      {
        int_info_t *int_info = (int_info_t *)info;
        instance_t *n, *m;
        intmach_t size = SwitchSize(int_info->indexer);

        for (n = int_info->first; n; n=m) {
          m=n->forward;
          n->rank = ERRORTAG;
          checkdealloc_FLEXIBLE_S(instance_t, objsize, n);
        }
        
        checkdealloc_FLEXIBLE(sw_on_key_t,
                              sw_on_key_node_t,
                              size,
                              int_info->indexer);
        
        checkdealloc_TYPE(int_info_t, info);
        break;
      }
    case TABLE:
      {
        sw_on_key_t *sw = (sw_on_key_t *)info;
        intmach_t size = SwitchSize(sw);
        checkdealloc_FLEXIBLE(sw_on_key_t,
                              sw_on_key_node_t,
                              size,
                              sw);
        break;
      }
    case EMUL_INFO:
      {
        free_emulinfo((emul_info_t *)info);
        break;
      }
    case OTHER_STUFF:
      {
        other_stuff_t *other = (other_stuff_t *)info;
        checkdealloc((tagged_t *)other->pointer, other->size);
        checkdealloc_TYPE(other_stuff_t, info);
        break;
      }
    default:
      break;
    }
}

/* JFMC: abolish/1 predicate calls abolish C function. */
CBOOL__PROTO(prolog_abolish)
{
  tagged_t *junk;
  definition_t *f;

  DEREF(X(0),X(0));
  f = find_definition(predicates_location,X(0),&junk,FALSE);
  return abolish(Arg, f);  
}

/* JFMC: abolish is now a C function instead of a predicate. */
/* Make a predicate undefined.  Also, forget spypoints etc. */
CBOOL__PROTO(abolish, definition_t *f)
{
  intmach_t current_mem = total_mem_count;
  /* MCL: abolish/1 must succeed even in the case of undefined predicates */
  if (!f) return TRUE; 
  if (/*f->predtyp == ENTER_C || */                               /* JFMC */
      f->predtyp > ENTER_INTERPRETED) return FALSE;
  if (f->predtyp != ENTER_UNDEFINED) {
    f->properties.spy = 0;
    f->properties.breakp = 0;
    make_undefined(Arg, f);
    num_of_predicates--;
  }
  INC_MEM_PROG(total_mem_count - current_mem);
  return TRUE;
}

CBOOL__PROTO(define_predicate)
{
  definition_t *f;
  enter_instr_t type;
  intmach_t current_mem = total_mem_count;

  DEREF(X(0),X(0));
  if ((f=parse_definition(X(0)))==NULL)
    USAGE_FAULT("$define_predicate: bad 1st arg");

  /*if (f->properties.public) return FALSE;*/

  if (f->properties.multifile){   /* DCG */
    INC_MEM_PROG(total_mem_count - current_mem);
    return TRUE;
  }

  if (f->predtyp!=ENTER_UNDEFINED)
    make_undefined(Arg,f);
  
  num_of_predicates++;      /* Decremented by make_undefined(), if called */

  DEREF(X(1),X(1));
  type = (X(1)==atom_unprofiled ?  ENTER_COMPACTCODE :
          X(1)==atom_profiled   ? ENTER_PROFILEDCODE :
          ENTER_INTERPRETED);

  switch (type) {
  case ENTER_INTERPRETED:
    init_interpreted(f);
    break;
  default:
    {
      incore_info_t *d;
      
      d = checkalloc_TYPE(incore_info_t);
      
      d->clauses.ptr = NULL;
      d->clauses_tail = &d->clauses;
      d->varcase = fail_alt;
      d->lstcase = NULL;        /* Used by native preds to hold nc_info */
      d->othercase = NULL; /* Used by native preds to hold index_clause */
#if defined(CACHE_INCREMENTAL_CLAUSE_INSERTION)
      d->last_inserted_clause = NULL;
      d->last_inserted_num = ~0;
#endif
      f->code.incoreinfo = d;
    }
    f->properties.nonvar = 0;
    f->properties.var = 0;
    SetEnterInstr(f,type);
    break;
  }
  INC_MEM_PROG(total_mem_count - current_mem);
  return TRUE;
}

CBOOL__PROTO(erase_clause)
{
  intmach_t current_mem = total_mem_count;

  DEREF(X(0),X(0));
  free_emulinfo(TaggedToEmul(X(0)));
  INC_MEM_PROG(total_mem_count - current_mem);

  return TRUE;
}

CBOOL__PROTO(clause_number)
{
  definition_t *f;
  uintmach_t number;

  DEREF(X(0),X(0));
  if ((f=parse_definition(X(0)))==NULL)
    USAGE_FAULT("$clause_number: bad 1st arg");

  number = f->code.incoreinfo->clauses_tail->number;

  CBOOL__UnifyCons(MakeSmall(number),X(1));
  return TRUE;
}

CBOOL__PROTO(compiled_clause)
{
  definition_t *f;
  emul_info_t *ref;
  unsigned int type;
  tagged_t t1, key;
  incore_info_t *d;
  unsigned int bitmap;
  emul_info_t *ep, **epp;
  intmach_t current_mem = total_mem_count;

  DEREF(X(0),X(0));             /* Predicate spec */
  if ((f=parse_definition(X(0)))==NULL)
    USAGE_FAULT("$emulated_clause: bad 1st arg");
  DEREF(X(1),X(1));             /* Bytecode object */
  ref = TaggedToEmul(X(1));
  DEREF(X(2),X(2));             /* Mode */
  DEREF(X(3),X(3));             /* f(Type,Key[,Base,Woff,Roff]) */
  DerefArg(t1,X(3),1);
  type = GetSmall(t1);
  DerefArg(key,X(3),2);
  if (IsVar(key))
    key = ERRORTAG;
  else if (TaggedIsSTR(key))
    key = TaggedToHeadfunctor(key);

                                /* add a new clause. */
  d = f->code.incoreinfo;

  ep = (emul_info_t *)d->clauses_tail; /* TODO: (JFMC) assumes that &clauses_tail->next == clauses_tail */
  if (d->clauses.ptr != NULL && IS_CLAUSE_TAIL(ep)) {
    for (epp = &d->clauses.ptr; *epp != ep; epp = (emul_info_t **)*epp)
      ;
    ref->subdefs = ep->subdefs;
    ref->next.ptr = ep->next.ptr;
    *epp = ref;
    checkdealloc_FLEXIBLE(emul_info_t,
                          char,
                          CLAUSE_TAIL_INSNS_SIZE,
                          ep);
  } else {
    ref->next.number = d->clauses_tail->number + 1;
    d->clauses_tail->ptr = ref;
  }
  d->clauses_tail = &ref->next;

  bitmap = 
    (type&0x1 &&  !f->properties.nonvar ? 0x1 : 0) | /* var   */
    (type&0x8 &&  !f->properties.var    ? 0x2 : 0) | /* lst   */
    (type&0x16 && !f->properties.var    ? 0x4 : 0) ; /* other */
  if ((type&0x21) == 0x21)
    f->properties.nonvar = 1;
  if ((type&0x3e) == 0x3e)
    f->properties.var = 1;

  if (!(f->predtyp&1) && bitmap!=0x7) {
    SetEnterInstr(f,f->predtyp+1);
    d->lstcase = incore_copy(d->varcase);
    d->othercase = new_switch_on_key(2,incore_copy(d->varcase));
  }

  if (!(f->predtyp&1))
    incore_insert(&d->varcase,f->arity,ref,d);
  else {
    if (bitmap&0x1)
      incore_insert(&d->varcase,f->arity,ref,d);
    if (bitmap&0x2)
      incore_insert(&d->lstcase,f->arity,ref,d);
    if (bitmap&0x4)
      incore_puthash(&d->othercase,f->arity,ref,d,key);
  }
  INC_MEM_PROG(total_mem_count - current_mem);
  return TRUE;
}

sw_on_key_node_t *dyn_puthash(sw_on_key_t **swp, tagged_t k)
{
  sw_on_key_node_t *h1;

  h1 = incore_gethash(*swp,k);
  if (h1->key)
    return h1;
  else {
    h1->key = k;
    if (((*swp)->count+=1)<<1 <= SwitchSize(*swp))
      return h1;
    else {
      expand_sw_on_key(swp,NULL,TRUE);
      return incore_gethash(*swp,k);
    }
  }
}

CBOOL__PROTO(set_property)
{
  definition_t *f;
  tagged_t *junk;
  enter_instr_t type;

  DEREF(X(0),X(0));
  f = find_definition(predicates_location,X(0),&junk,FALSE);
  if (f == NULL) return FALSE;
  type = f->predtyp;
  if ((type > ENTER_FASTCODE_INDEXED && type != ENTER_INTERPRETED) ||
      (type <= ENTER_FASTCODE_INDEXED && f->code.incoreinfo->clauses.ptr != NULL) ||
      (type == ENTER_INTERPRETED && f->code.intinfo->first))
    return FALSE;

  DEREF(X(1),X(1));
  /*
    if (X(1)==atom_public)
    f->properties.public = 1;
    else
    f->properties.public = 0;
  */
  if (X(1)==atom_wait) {
    f->properties.wait = 1;
    SetEnterInstr(f,type);
  } else if ((X(1)==atom_dynamic) || (X(1) == atom_concurrent)) {  /* MCL */
    f->properties.dynamic = 1;
    f->properties.concurrent = X(1) == atom_concurrent;            /* MCL */

    if (type != ENTER_INTERPRETED) {          /* Change it to interpreted */
      free_incoreinfo(&f->code.incoreinfo);
      init_interpreted(f);
    }

    /* In any case, set the runtime behavior */
    f->code.intinfo->behavior_on_failure =
#if defined(USE_THREADS)
      f->properties.concurrent ? CONC_OPEN : DYNAMIC;
#else
      DYNAMIC;
#endif
  } else if (X(1)==atom_multifile) {
    f->properties.multifile = 1;
  }

  return TRUE;
}

/* --------------------------------------------------------------------------- */
/*  Tasks (goal_descriptor_t). */

/* Cross-link a WAM and a goal descriptor */
static void associate_wam_goal(worker_t *w, goal_descriptor_t *goal_desc) {
  goal_desc->worker_registers = w;
  w->misc->goal_desc_ptr = goal_desc;
}

/* Cross-link a WAM and a goal descriptor */
static void dissociate_wam_goal(worker_t *w, goal_descriptor_t *goal_desc) {
  /* dissociate the WAM from the goal descriptor */
  goal_desc->worker_registers = NULL;
  w->misc->goal_desc_ptr = NULL;
}

/* If we are not using threads, this simply points to a single WRB state;
   i.e., the list is actually a singleton. */
/* wrb_state_p wrb_state_list;  */

SLOCK goal_desc_list_l;
goal_descriptor_t *goal_desc_list = NULL;

SLOCK thread_to_free_l;
THREAD_T thread_to_free = (THREAD_T)NULL;

uintmach_t global_goal_number = 0;                 /* Last number taken */


/* The initial list has a single goal descriptor with no WAM and in
   IDLE state */

void init_goal_desc_list(void)
{
  Init_slock(goal_desc_list_l);
  Init_slock(thread_to_free_l);
  
  goal_desc_list = checkalloc_TYPE(goal_descriptor_t);
  goal_desc_list->state = IDLE;
  goal_desc_list->worker_registers = NULL;
  Init_slock(goal_desc_list->goal_lock_l);
  goal_desc_list->forward = goal_desc_list->backward = goal_desc_list;
}

uintmach_t num_tasks_created(void) {
  return global_goal_number;
}

/* "goal" is to be the only working thread in the system.  The rest of the
   goals have no associated thread */

void reinit_list(goal_descriptor_t *myself)
{
     goal_descriptor_t *goal_ref;
     
     Wait_Acquire_slock(goal_desc_list_l);
     goal_ref = goal_desc_list;
     do {
       if ((goal_ref != myself)) {
         unlink_wam(goal_ref);
         goal_ref->state = IDLE;
       }
       goal_ref = goal_ref->forward;
     } while (goal_ref != goal_desc_list);
     goal_desc_list = myself->forward;
     Release_slock(goal_desc_list_l);
}


 /* Try to kill a goal (and release the wam it was attached to).
 Returns 0 if no error, -1 on error (maybe no such thread).  Actually,
 killing a thread should be done otherwise: the killed thread might be
 in an unstable state in which it should not be killed. */

/*
int kill_thread(goal_descriptor_t *this_goal)
{
  return Thread_Cancel(this_goal->thread_handle);
}
*/

/* Cause kills to this thread to be immediately executed */
void allow_thread_cancellation(void)
{
  Allow_Thread_Cancel;
}


/* Cause kills to this thread to be ignored (for symmetry with the above) */
void disallow_thread_cancellation(void)
{
  Disallow_Thread_Cancel;
}

/* Should be called after the list is inited */
goal_descriptor_t *init_first_gd_entry(void)
{
  goal_descriptor_t *first_gd;

  first_gd = gimme_a_new_gd();

  first_gd->thread_id = Thread_Id;
  first_gd->thread_handle = (THREAD_T)NULL; /* Special case? */
  first_gd->action = NO_ACTION;
  first_gd->goal = (tagged_t)NULL;
  
  return first_gd;
}

/* Returns a free goal descriptor, with a WAM attached to it.  If
   needed, create memory areas, initialize registers, etc. The worker
   is marked as WORKING in order to avoid other threads stealing it.
   The WAM areas are already initialized. */

goal_descriptor_t *gimme_a_new_gd(void)
{
  goal_descriptor_t *gd_to_run;

  gd_to_run = look_for_a_free_goal_desc();
  if (gd_to_run != NULL) {
    /* Make sure it has a WAM */
    if (gd_to_run->worker_registers == NULL) {
      associate_wam_goal(free_wam(), gd_to_run);
    }
  } else {
    gd_to_run = attach_me_to_goal_desc_list(free_wam());
  }
  return gd_to_run;
}


/* We already have a WAM.  Create a goal descriptor in WORKING state,
   add it to the goal descriptor list, and mark it as ours.  */

CFUN__PROTO(attach_me_to_goal_desc_list, goal_descriptor_t *)
{
  goal_descriptor_t *goal_desc_p;

  goal_desc_p = checkalloc_TYPE(goal_descriptor_t);
  goal_desc_p->state = WORKING;
  goal_desc_p->goal_number = ++global_goal_number;
  Init_slock(goal_desc_p->goal_lock_l);
  associate_wam_goal(Arg, goal_desc_p);

  /* Add it at the end of the list, i.e., add it to the "backward"
     side of the list, where the non-free goal descriptors are. */
  Wait_Acquire_slock(goal_desc_list_l);
  goal_desc_p->forward = goal_desc_list;
  goal_desc_p->backward = goal_desc_list->backward;
  goal_desc_list->backward->forward = goal_desc_p;
  goal_desc_list->backward = goal_desc_p;
  Release_slock(goal_desc_list_l);
  return goal_desc_p;
}

/* The WAM used by goal is not to be used any more.  Remove the
   choicepoints and the possible dynamic concurrent choicepoints,
   unlink it from the goal descriptor and return it to the free wam
   list. */

void unlink_wam(goal_descriptor_t *goal)
{
  worker_t *w;

  w = goal->worker_registers;
  if (w != NULL) {
#if defined(USE_THREADS)            /* Clean the possible conc. chpt. */
    remove_link_chains(&TopConcChpt, InitialChoice);
#endif
    dissociate_wam_goal(w, goal);
    release_wam(w);
  }
}

/* A goal descriptor state is to be marked as free --- no thread is
   working on it.  It is not, however, deleted from the state list, or
   the WAM freed, for creating areas is a costly process.  However, we
   move it to the beginning of the list.  We should have exclusive
   access to it, therefore I do not protect the goal descriptor areas.
   The WAM is put back in the lis of available WAMs. */

void make_goal_desc_free(goal_descriptor_t *goal)
{
  unlink_wam(goal);             /* Clean WAM, put it back to free list */

  Wait_Acquire_slock(goal_desc_list_l);
  //  fprintf(stderr, "MAKE GOAL DESC FREE %p\n", goal);
  goal->state = IDLE;
                                /* Unlink from current place */
  goal->backward->forward = goal->forward;
  goal->forward->backward = goal->backward;
                                /* Link at the beginning */
  goal->forward = goal_desc_list;
  goal->backward = goal_desc_list->backward;
  goal_desc_list->backward->forward = goal;
  goal_desc_list->backward = goal;
  goal_desc_list = goal;

  Release_slock(goal_desc_list_l);
}

/* TODO: this can be implemented more easily by declaring thread-local
   variables, supported by GCC -- JFMC */
/* What goal descriptor am I working for, if I do not know which is my
   WAM?  Its use is only justified when we are recovering from an
   interruption */

worker_t *get_my_worker(void)
{
  THREAD_ID thr_id = Thread_Id;
  goal_descriptor_t *this_goal;

  /* Freeze the status of the goal descriptor list */

  Wait_Acquire_slock(goal_desc_list_l);
  this_goal = goal_desc_list;

  /* Go backwards: the goals at the beginning are free. */

  while( (this_goal != goal_desc_list) &&
         ((this_goal->state != WORKING ) ||
          !Thread_Equal(this_goal->thread_id, thr_id)))
    this_goal = this_goal->backward;
  Release_slock(goal_desc_list_l);
  
  if (Thread_Equal(this_goal->thread_id, thr_id) &&
      (this_goal->state == WORKING))
    return this_goal->worker_registers;
  else
    SERIOUS_FAULT("Could not find goal descriptor");    
}


/* Return a free goal descriptor from the ring.  Mark it as WORKING as
   soon as we find one free so that no other thread can steal it. If
   there is any free descriptor, it should appear at the beginning of
   the chain.
*/

goal_descriptor_t *look_for_a_free_goal_desc(void)
{
  goal_descriptor_t *free_goal_desc;

  Wait_Acquire_slock(goal_desc_list_l);
  if (goal_desc_list->state == IDLE) {
    free_goal_desc = goal_desc_list;
    goal_desc_list = goal_desc_list->forward; 
    free_goal_desc->state = WORKING;
    free_goal_desc->goal_number = ++global_goal_number;
    /* Init_slock(free_goal_desc->goal_lock_l); */
  } else free_goal_desc = NULL;
  Release_slock(goal_desc_list_l);
  return free_goal_desc;
}


void enqueue_thread(THREAD_T thread)
{
  Wait_Acquire_slock(thread_to_free_l);
  if (thread_to_free) 
    Thread_Join(thread_to_free);
  thread_to_free = thread;
  Release_slock(thread_to_free_l);
}

/* --------------------------------------------------------------------------- */
/* Support code for starting goal execution. */

/* Here with w->next_insn set up -- see local_init_each_time(). (MCL) */

SIGJMP_BUF abort_env; /* Shared */

CBOOL__PROTO(prolog_eng_killothers);

CFUN__PROTO(call_firstgoal, intmach_t, tagged_t goal_term, goal_descriptor_t *goal_desc) {
  int i, exit_code;

  w->choice->x[0] = X(0) = goal_term;
  w->next_insn = bootcode;

  while(TRUE) {
    i = SIGSETJMP(abort_env);
    if (i == 0){                /* Just made longjmp */
      w->x[0] = w->choice->x[0];
      in_abort_context = TRUE;
      wam(w, goal_desc);
      w = goal_desc->worker_registers; /* segfault patch -- jf */
      exit_code = w->misc->exit_code;
#if 0
      flush_output(w);
#endif
      if (exit_code != WAM_ABORT) /* halting... */
        break;
    } else if (i == -1) {         /* SIGINT during I/O */
      frame_t *e;
      /* No need to patch "p" here, since we are not exiting wam() */
      bcp_t p = (bcp_t)int_address;
      int_address = NULL;
      SETUP_PENDING_CALL(E, address_true);
      continue;
    }
#if defined(USE_THREADS)
    prolog_eng_killothers(w);
#endif

    {
      in_abort_context = FALSE;                    /* disable recursive aborts */
      reinitialize_wam_areas(w); /* aborting... */
      empty_gcdef_bin(w); /* TODO: here? */
      fflush(stdout); /* TODO: here? */
      fflush(stderr); /* TODO: here? */
      in_abort_context = TRUE;
    }

    init_each_time(w);                /* Sets X(0) to point to bootcode */
    {
      /* Patch predicate call at bootcode */
      bcp_t P = bootcode;
      P = BCoff(P, FTYPE_size(f_o)); /* CALLQ */
      P = BCoff(P, FTYPE_size(f_Q)); /* 0 */
      EMIT_E(address_restart);
    }
  }
  return exit_code;
}

/* Here with wam and goal */

THREAD_RES_T startgoal(THREAD_ARG wo)
{
  worker_t *w;
  goal_descriptor_t *goal_desc = (goal_descriptor_t *)wo;
  int result_state;
  int wam_result;

  w = goal_desc->worker_registers;
  w->next_insn = startgoalcode;
  SetDeep();  /* Force backtracking after alts. exahusted */
  w->choice->x[0] = X(0);    /* Will be the arg. of a call/1 */
 
#if defined(DEBUG_TRACE) && defined(USE_THREADS)
  if (debug_threads)
    printf("%" PRIdm " (%" PRIdm ") Goal %p (with starting point 0x%" PRIxm ") entering wam()\n",
           (intmach_t)Thread_Id, (intmach_t)GET_INC_COUNTER, 
           goal_desc, (intmach_t)X(0));
#endif

  wam(Arg, goal_desc);    /* segfault patch -- jf */
  w = goal_desc->worker_registers;
  wam_result = w->misc->exit_code;

#if defined(DEBUG_TRACE) && defined(USE_THREADS)
  if (debug_threads)
    printf("%" PRIdm " (%" PRIdm ") Goal %p (with starting point 0x%" PRIxm ") exiting wam()\n",
           (intmach_t)Thread_Id, (intmach_t)GET_INC_COUNTER, 
           goal_desc, (intmach_t)X(0));
#endif

  if (wam_result == WAM_ABORT) {
    MAJOR_FAULT("Wam aborted!");
  }
  
#if defined(DEBUG_TRACE) && defined(USE_THREADS)
  if (debug_threads)
    printf("% " PRIdm " (%" PRIdm ") Goal %p exited wam()\n", 
           (intmach_t)Thread_Id, (intmach_t)GET_INC_COUNTER, goal_desc);
#endif
#if 0
  flush_output(Arg);
#endif

  /* eng_wait() may change NEEDS_FREEING and consults the state of the
     thread; therefore we lock until it is settled down */
  
  Wait_Acquire_slock(goal_desc->goal_lock_l);
  if (goal_desc->worker_registers->next_alt == termcode){
    unlink_wam(goal_desc);      /* We can make the WAM available right now */
    goal_desc->state = FAILED;
  } else goal_desc->state = PENDING_SOLS;

/* In some cases (i.e., Win32) the resources used up by the thread are
   not automatically freed upon thread termination.  If needed, the
   thread handle is enqued. In an (I hope) future implementation the
   thread will go to sleep instead of dying. */

  if (goal_desc->action & NEEDS_FREEING){ /* Implies thread created */
#if defined(DEBUG_TRACE) && defined(USE_THREADS)
    if (debug_threads) printf("Goal %p enqueuing itself\n", goal_desc);
#endif
    enqueue_thread(goal_desc->thread_handle); /* Free, enqueue myself */
  } else   
    enqueue_thread((THREAD_T)NULL); /* Free whoever was there, enqueue no one*/
  
/* Save the state for the exit result (if we release the goal, its
   state may change before we return from the function). */
  result_state = goal_desc->state;

/* Goals failed when executed by the local thread, and goals for which
   no Id was requested, release the memory areas automatically */
  if ((wam_result == WAM_INTERRUPTED) ||
      !(goal_desc->action & KEEP_STACKS) ||
      ((goal_desc->state == FAILED) && !(goal_desc->action & CREATE_THREAD)))
    make_goal_desc_free(goal_desc);

  Release_slock(goal_desc->goal_lock_l);

#if defined(DEBUG_TRACE) && defined(USE_THREADS)
  if (debug_threads || debug_conc)
    printf("*** %" PRIdm " (%" PRIdm ") Goal %p is EXITING\n", (intmach_t)Thread_Id, (intmach_t)GET_INC_COUNTER, goal_desc);
#endif
  return (THREAD_RES_T)(uintptr_t)(result_state == PENDING_SOLS);
}


/* If we hit the initial ghost choicepoint, then it means that no
   solution was returned by this call.  If we call the
   make_backtracking() primitive, then KEEP_STACKS is true. */

THREAD_RES_T make_backtracking(THREAD_ARG wo)
{
  goal_descriptor_t *goal_desc = (goal_descriptor_t *)wo;
  int result_state;
  int wam_result;
  worker_t *w = goal_desc->worker_registers;

  /* segfault patch -- jf */
  wam(Arg, goal_desc);
  Arg = goal_desc->worker_registers;
  wam_result = goal_desc->worker_registers->misc->exit_code;
  if (wam_result == WAM_ABORT) {
    MAJOR_FAULT("Wam aborted while doing backtracking");
  }

#if 0
  flush_output(Arg);
#endif

  Wait_Acquire_slock(goal_desc->goal_lock_l);
  if (Arg->next_alt == termcode) {
    unlink_wam(goal_desc);
    goal_desc->state = FAILED;
  } else goal_desc->state = PENDING_SOLS;

  if ((goal_desc->action & NEEDS_FREEING) ||
      (wam_result == WAM_INTERRUPTED)) /* Implies thread created */
    enqueue_thread(goal_desc->thread_handle); /* Free, enqueue myself */
  else   
    enqueue_thread((THREAD_T)NULL); /* Free whoever was there, enqueue no one*/

  result_state = goal_desc->state;

  /*
  if ((goal_desc->state == FAILED) && !(goal_desc->action & CREATE_THREAD))
    make_goal_desc_free(goal_desc);
  */

  Release_slock(goal_desc->goal_lock_l);

  return (THREAD_RES_T)(uintptr_t)(result_state == PENDING_SOLS);
}

/* --------------------------------------------------------------------------- */

#if defined(USE_GLOBAL_VARS)
CBOOL__PROTO(prolog_global_vars_set_root) {
  /* Warning: program must not backtrack after executing this code */
  DEREF(GLOBAL_VARS_ROOT,X(0));
  return TRUE;
}
CBOOL__PROTO(prolog_global_vars_get_root) {
  DEREF(X(0),X(0));
  CBOOL__LASTUNIFY(GLOBAL_VARS_ROOT, X(0));
}
#else
CBOOL__PROTO(prolog_global_vars_set_root) {
  USAGE_FAULT("This engine was not compiled with global variables support (USE_GLOBAL_VARS)");
}
CBOOL__PROTO(prolog_global_vars_get_root) {
  USAGE_FAULT("This engine was not compiled with global variables support (USE_GLOBAL_VARS)");
}
#endif

/* ------------------------------------------------------------------------- */
/* BUILTIN C PREDICATES */

// #if defined(INTERNAL_CALLING)
// CBOOL__PROTO(prolog_internal_call)
// {
//   bcp_t next_insn;
// 
//   printf("In current_executable, internal_calling is 0x%p\n", 
//          address_internal_call);  
//   next_insn = Arg->next_insn;
//   Arg->next_insn = internal_calling;
//   wam(Arg, NULL);
//   Arg->next_insn = next_insn;
//   return TRUE;
// }
// #endif

CBOOL__PROTO(prompt)
{
  CBOOL__UnifyCons(current_prompt,X(0));
  DEREF(current_prompt,X(1)); 
  return TRUE;
}

CBOOL__PROTO(unknown)
{
  CBOOL__UnifyCons(current_unknown,X(0));
  DEREF(current_unknown,X(1)); 
  return TRUE;
}


/* $setarg(+I, +Term, +Newarg, +Mode):
 * Replace (destructively) I:th arg of Term by Newarg.
 * Mode=on -> dereference, undo on backtracking;
 * Mode=off -> we're undoing now, don't dereference;
 * Mode=true -> dereference, don't undo later.
 *
 * Put in at the express request of Seif Haridi.
 */
CBOOL__PROTO(setarg)
{
  tagged_t t1, t2, *ptr;
  tagged_t oldarg, number, complex, newarg, *x;
  
  number = X(0);
  complex = X(1);
  newarg = X(2);
  DEREF(X(3),X(3));
  
  if (X(3) != atom_off) {
    DerefSw_HVAorCVAorSVA_Other(number,{goto barf1;},{});
    DerefSw_HVAorCVAorSVA_Other(complex,{goto barf2;},{});
    DerefSw_HVAorCVAorSVA_Other(newarg,{goto unsafe_value;},{});
  } else {
  unsafe_value:
    if (TaggedIsSVA(newarg)){
      ptr = w->heap_top;
      LoadHVA(t1,ptr);
      w->heap_top = ptr;
      BindSVA(newarg,t1);
      newarg = t1;
    }
  }
  
  if (TaggedIsSTR(complex)) {
    intmach_t i = GetSmall(number);
    tagged_t f = TaggedToHeadfunctor(complex);
    
    if (i<=0 || i>Arity(f) || f&QTAGMASK)
      goto barf1;
    
    ptr = TaggedToArg(complex,i);
  } else if (IsComplex(complex)){       /* i.e. list */
    if (number==MakeSmall(1))
      ptr = TaggedToCar(complex);
    else if (number==MakeSmall(2))
      ptr = TaggedToCdr(complex);
    else
      goto barf1;
  } else goto barf2;
  
  RefHeap(oldarg,ptr);
  *ptr = newarg;
  
  if ((X(3)==atom_on) && CondHVA(Tagp(HVA,ptr))) {
    /* undo setarg upon backtracking */
    tagged_t *limit = TrailTopUnmark(w->choice->trail_top);
    
    /* check first if location already trailed is same segment */
    t1 = Tagp(HVA,ptr);
    
    for (x=w->trail_top; TrailYounger(x,limit);) {
      TrailDec(x);
      t2 = *x; // (x points to the popped element)
      if (t1 == t2)
        return TRUE;
    }
    
    ptr = w->heap_top;
    t2 = Tagp(STR,ptr);
    HeapPush(ptr,functor_Dsetarg);
    HeapPush(ptr,number);
    HeapPush(ptr,complex);
    HeapPush(ptr,oldarg);
    HeapPush(ptr,atom_off);
    TrailPush(w->trail_top,t2);
    w->heap_top = ptr;
    
    /* trail smashed location for segmented GC */
    TrailPush(w->trail_top,t1);
    
    if (ChoiceYounger(ChoiceOffset(w->choice,CHOICEPAD),w->trail_top))
      choice_overflow(Arg,2*CHOICEPAD*sizeof(tagged_t),TRUE);
  }
  
  return TRUE;
  
 barf1:
  MINOR_FAULT("setarg/3: incorrect 1st argument");
  
 barf2:
  MINOR_FAULT("setarg/3: incorrect 2nd argument");
}

CBOOL__PROTO(undo)
{
  tagged_t goal;
  
  goal = X(0);
  DerefSw_HVAorCVAorSVA_Other(goal,{MINOR_FAULT("$undo/1: invalid argument");},{});
  TrailPush(w->trail_top,goal);
  if (ChoiceYounger(ChoiceOffset(w->choice,CHOICEPAD),w->trail_top))
    choice_overflow(Arg,2*CHOICEPAD*sizeof(tagged_t),TRUE);
  return TRUE;
}

/* x1 = constraints on x0, or '[]' if there are none */
CBOOL__PROTO(frozen)
{
  DEREF(X(0),X(0));
  if (!IsVar(X(0)))
    return FALSE;
  else if (VarIsCVA(X(0)))
    CBOOL__LASTUNIFY(Tagp(LST,TaggedToGoal(X(0))),X(1));
  CBOOL__UnifyCons(atom_nil,X(1));
  return TRUE;
}

CBOOL__PROTO(defrost)
{
  tagged_t t;
  tagged_t *h = w->heap_top;
  
  DEREF(X(0),X(0));
  DEREF(X(1),X(1));
  if (X(1)==atom_nil)
    {
      LoadHVA(t,h);
    }
  else
    {
      LoadCVA(t,h);
      HeapPush(h,*TaggedToCar(X(1)));
      HeapPush(h,*TaggedToCdr(X(1)));
    }
  BindCVANoWake(X(0),t);
  w->heap_top = h;
  return TRUE;
}

/* --------------------------------------------------------------------------- */

CBOOL__PROTO(compiling)
{
  CBOOL__UnifyCons(current_compiling,X(0));
  DEREF(X(1),X(1));
  if (
      X(1)!=atom_unprofiled 
#if defined(GAUGE)
      && X(1)!=atom_profiled 
#endif
      )
    return FALSE;

  current_compiling = X(1);
  return TRUE;
}

CBOOL__PROTO(ferror_flag)
{
  CBOOL__UnifyCons(current_ferror_flag,X(0));
  DEREF(current_ferror_flag,X(1)); 
  return TRUE;
}

CBOOL__PROTO(quiet_flag)
{
  CBOOL__UnifyCons(current_quiet_flag,X(0));
  DEREF(current_quiet_flag,X(1)); 
  return TRUE;
}

CBOOL__PROTO(prolog_radix)
{
  CBOOL__UnifyCons(current_radix,X(0));
  DEREF(current_radix,X(1));
  return TRUE;
}

/* --------------------------------------------------------------------------- */

CFUN__PROTO(find_constraints, intmach_t, tagged_t *limit);

CBOOL__PROTO(constraint_list)
{
  intmach_t pad;
  tagged_t *h;
  tagged_t l, v, clist;
  
  pad = HeapCharDifference(w->heap_top,Heap_End);
  DEREF(X(0),X(0));
  while ((find_constraints(Arg, TaggedToPointer(X(0)))*LSTCELLS)*sizeof(tagged_t)+CONTPAD > pad) {
    l = *w->trail_top;
    while (l!=atom_nil) {
      v = l;
      l = *TagpPtr(CVA,v);
      *TagpPtr(CVA,v) = v;
    }
    /* TODO: use pad<<=1 here or recompute available? */
    pad <<= 1;
    explicit_heap_overflow(Arg,pad*2,2);
  }
  h = w->heap_top;
  l = *w->trail_top;
  clist = atom_nil;
  while (l!=atom_nil) {
    v = l;
    l = *TagpPtr(CVA,v);
    *TagpPtr(CVA,v) = v;
    HeapPush(h,v);
    HeapPush(h,clist);
    clist = Tagp(LST,HeapOffset(h,-2));
  }
  w->heap_top = h;
  CBOOL__LASTUNIFY(clist,X(1));
}

CFUN__PROTO(find_constraints, intmach_t, tagged_t *limit)
{
  choice_t *purecp; /* oldest CVA-free cp */
  choice_t *cp;
  intmach_t found = 0;
  
  cp = purecp = ChoiceNext0(w->choice,0);
  cp->next_alt = fail_alt;
  CHPTFLG(cp->flags = 0);
  cp->trail_top = w->trail_top;
  cp->heap_top = w->heap_top;
  *w->trail_top = atom_nil;
  while (limit < (tagged_t *)NodeGlobalTop(cp))
    {
      choice_t *prevcp = ChoiceCont(cp);
      
      if (1 /* !ChoiceptTestNoCVA(cp)*/)
        {
          tagged_t *h = NodeGlobalTop(prevcp); 

          if (h<limit) h = limit;
          while (h < (tagged_t *)NodeGlobalTop(cp))
            {
              tagged_t v = *h++;
              
              if (v&QTAGMASK) h += LargeArity(v);
              else if (TaggedIsATM(v)) h += Arity(v);
              else if (v==Tagp(CVA,h-1))
                {
                  h[-1] = *w->trail_top;
                  *w->trail_top = v;
                  found++;
                  h += 2;
                  purecp = prevcp;
                }
            }
          /* Christian Holzbaur pointed out that this is unsafe, e.g.
             | ?- dif(X,1), (true; fail), (X=2; frozen(X,Fr)).
          if (purecp!=prevcp && limit<=NodeGlobalTop(prevcp))
            ChoiceptMarkNoCVA(cp); */
        }
      
      cp = prevcp;
    }
  
  return found;
}

/* --------------------------------------------------------------------------- */

/* support for circularity checks:
   $eq(X,Y) :- X==Y, occupies_same_location(X,Y).
 */
CBOOL__PROTO(prolog_eq)
{
  DEREF(X(0),X(0));
  DEREF(X(1),X(1));
  
  return (X(0)==X(1));
}

/* --------------------------------------------------------------------------- */

/* from bc_aux.h */
CFUN__PROTO(compile_term_aux, instance_t *,
            tagged_t head, 
            tagged_t body, 
            worker_t **new_worker);

int_info_t *current_clauses_aux(tagged_t head);
bool_t insertz_aux(int_info_t *root, instance_t *n);

CBOOL__PROTO(prolog_interpreted_clause)
{
  tagged_t Head, Body, t;
  /*tagged_t ListHB;*/
  instance_t *object;
  int_info_t *root;
  worker_t *new_worker;

  DEREF(t, X(1));

  if (TaggedIsSTR(t) && (TaggedToHeadfunctor(t) == functor_neck)) {
    DerefArg(Head,t,1);
    DerefArg(Body,t,2);    
    
    new_worker = NULL;
    object = compile_term_aux(Arg, Head, Body, &new_worker);
#if defined(DEBUG_TRACE)
      if (new_worker)
        fprintf(stderr, "wrb reallocation in prolog_interpreted_clause()\n");
#endif
    if ((root = current_clauses_aux(Head)) == NULL) 
      MAJOR_FAULT("Root == NULL @ c_interpreted_clause!!!");

    return insertz_aux(root, object);

  } else return FALSE;
}

int_info_t *current_clauses_aux(tagged_t head)
{
  if (!IsVar(head)) {
    tagged_t *junk;
    definition_t *d = 
      find_definition(predicates_location, head, &junk, FALSE);
    
    if ((d!=NULL) && (d->predtyp==ENTER_INTERPRETED))
      return d->code.intinfo;
  }
  return NULL; 
}

/* --------------------------------------------------------------------------- */

#if defined(ATOMGC)
CBOOL__PROTO(prolog_erase_atom)
{
  intmach_t index;

  DEREF(X(0), X(0));
  index = IndexPart(X(0));

#if defined(DEBUG_TRACE)
  /*  printf("erasing atom %s at %ld\n", atmtab[index]->value.atomp->name, index);*/
#endif

/* atmtab[i] point to parts of other data structure, so we fix the values
   there and then set a null pointer in atmtab[] */

/* 1 cannot be the key of any entry (see lookup_atom_idx()), and is used to
   mark a deleted entry (see atom_gethash()) */

  atmtab[index]->key = 1;
  atmtab[index]->value.atomp = NULL;
  atmtab[index] = NULL;
  ciao_atoms->count--;
  return TRUE;
}
#endif

/* --------------------------------------------------------------------------- */

extern char *ciao_version;
extern char *ciao_patch;
extern char *ciao_commit_branch;
extern char *ciao_commit_id;
extern char *ciao_commit_date;
extern char *ciao_commit_desc;

/*
 *  $ciao_version(?Major, ?Minor, ?Patch,
 *    ?CommitBranch, ?CommitId, ?CommitDate, ?CommitDesc) 
 *    for current_prolog_flag(version_data, ?V).
 */
CBOOL__PROTO(prolog_version) {
  DEREF(X(0), X(0));
  DEREF(X(1), X(1));
  DEREF(X(2), X(2));
  DEREF(X(3), X(3));
  DEREF(X(4), X(4));
  DEREF(X(5), X(5));
  DEREF(X(6), X(6));
  return (cunify(Arg, MakeSmall(CIAO_MAJOR_VERSION), X(0)) &&
          cunify(Arg, MakeSmall(CIAO_MINOR_VERSION), X(1)) &&
          cunify(Arg, MakeSmall(CIAO_PATCH_NUMBER),  X(2)) &&
          cunify(Arg, GET_ATOM(ciao_commit_branch),  X(3)) &&
          cunify(Arg, GET_ATOM(ciao_commit_id),      X(4)) &&
          cunify(Arg, GET_ATOM(ciao_commit_date),    X(5)) &&
          cunify(Arg, GET_ATOM(ciao_commit_desc),    X(6)));
}

/* --------------------------------------------------------------------------- */

CVOID__PROTO(show_nodes, choice_t *cp, choice_t *end);

CBOOL__PROTO(prolog_show_nodes)
{
  DEREF(X(0),X(0));
  DEREF(X(1),X(1));
  show_nodes(w, ChoiceFromTagged(X(0)), ChoiceFromTagged(X(1)));
  return TRUE;
}

CBOOL__PROTO(prolog_show_all_nodes)
{
  show_nodes(w, w->choice, InitialChoice);
  return TRUE;
}


CBOOL__PROTO(start_node)
{
  DEREF(X(0),X(0));
  CBOOL__UnifyCons(ChoiceToTagged(InitialChoice),X(0));
  return TRUE;
}

#if defined(DEBUG_NODE)
void display_functor(definition_t *functor) {
  if (functor) {
    if (IsString((functor)->printname)) {
      fprintf(stderr, "'%s'/", GetString((functor)->printname));
    } else {
      fprintf(stderr, "_/");
    }
    fprintf(stderr, "%d", (functor)->arity);
  } else {
    fprintf(stderr, "_F");
  }
}
#define DisplayCPFunctor(cp) display_functor(cp->functor)
#else
#define DisplayCPFunctor(cp) fprintf(stderr, "_N")
#endif

CVOID__PROTO(show_nodes, choice_t *cp_younger, choice_t *cp_older) {
  intmach_t number;
  try_node_t *next_alt;
#if !defined(DEBUG_NODE)
  fprintf(stderr, "/* functor information in nodes not available */\n");
#endif
  fprintf(stderr, "nodes(");
  fprintf(stderr,"0x%p:",cp_younger);
  DisplayCPFunctor(cp_younger);
  fprintf(stderr, ", ");
  fprintf(stderr, "[");
  // TODO:[oc-merge] was "cp_younger == w->choice && IsShallowTry(cp_younger)"
  if (cp_younger == w->choice && !IsDeep()) {
    next_alt = w->next_alt;
  } else {
    next_alt = cp_younger->next_alt;
  }
  number = next_alt->number;
  cp_younger = ChoiceCont0(cp_younger, next_alt->arity);
  while(ChoiceYounger(cp_younger, cp_older)) {
    fprintf(stderr,"\n  ");
    fprintf(stderr,"0x%p:",cp_younger);
    DisplayCPFunctor(cp_younger);
    fprintf(stderr, "/%" PRIdm ",", number);
    number = cp_younger->next_alt->number;
    cp_younger = ChoiceCont0(cp_younger, ChoiceArity(cp_younger));
  }
  if (!ChoiceYounger(cp_older, cp_younger)) {
    fprintf(stderr,"\n  ");
    fprintf(stderr,"0x%p:",cp_older);
    DisplayCPFunctor(cp_older);
    fprintf(stderr, "/%" PRIdm "\n",number);
  }
  fprintf(stderr, "])\n");
}

/* ------------------------------------------------------------------------- */

extern char *eng_architecture;
extern char *eng_os;
extern char *eng_debug_level;

CBOOL__PROTO(prolog_print_emulator_version) {
  CVOID__CALL(print_string, Output_Stream_Ptr, CIAO_VERSION_STRING);
  CVOID__CALL(print_string, Output_Stream_Ptr, " [");
  CVOID__CALL(print_string, Output_Stream_Ptr, eng_os);
  CVOID__CALL(print_string, Output_Stream_Ptr, eng_architecture);
  CVOID__CALL(print_string, Output_Stream_Ptr, "]");
  if (strcmp(eng_debug_level, "nodebug") != 0) {
    CVOID__CALL(print_string, Output_Stream_Ptr, " [");
    CVOID__CALL(print_string, Output_Stream_Ptr, eng_debug_level);
    CVOID__CALL(print_string, Output_Stream_Ptr, "]");
  }
  CVOID__CALL(print_string, Output_Stream_Ptr, "\n");
  return TRUE;
}

/*
CBOOL__PROTO(prolog_sourcepath)
{
  char cbuf[MAXPATHLEN];

  DEREF(X(0),X(0));
  strcpy(cbuf,source_path);
  strcat(cbuf,"/");
  strcat(cbuf,GetString(X(0)));
  CBOOL__UnifyCons(GET_ATOM(cbuf),X(1));
  return TRUE;
}
*/

