// The game's operator new and delete on the port's allocator (d3malloc.c,
// dlmalloc). Linked into the Amiga executables, they replace libstdc++'s,
// which go through libnix's slow free() (PLAN.md §9).
#include <cstddef>
#include <new>

extern "C" {
void *dlmalloc( size_t );
void dlfree( void * );
void *dlmemalign( size_t, size_t );
}

static void *allocate( size_t n ) {
	void *p = dlmalloc( n ? n : 1 );
	if ( !p ) {
		throw std::bad_alloc();
	}
	return p;
}

static void *allocate_aligned( size_t n, std::align_val_t a ) {
	void *p = dlmemalign( (size_t)a, n ? n : 1 );
	if ( !p ) {
		throw std::bad_alloc();
	}
	return p;
}

void *operator new( size_t n ) { return allocate( n ); }
void *operator new[]( size_t n ) { return allocate( n ); }
void *operator new( size_t n, const std::nothrow_t & ) noexcept { return dlmalloc( n ? n : 1 ); }
void *operator new[]( size_t n, const std::nothrow_t & ) noexcept { return dlmalloc( n ? n : 1 ); }
void *operator new( size_t n, std::align_val_t a ) { return allocate_aligned( n, a ); }
void *operator new[]( size_t n, std::align_val_t a ) { return allocate_aligned( n, a ); }

void operator delete( void *p ) noexcept { dlfree( p ); }
void operator delete[]( void *p ) noexcept { dlfree( p ); }
void operator delete( void *p, size_t ) noexcept { dlfree( p ); }
void operator delete[]( void *p, size_t ) noexcept { dlfree( p ); }
void operator delete( void *p, const std::nothrow_t & ) noexcept { dlfree( p ); }
void operator delete[]( void *p, const std::nothrow_t & ) noexcept { dlfree( p ); }
void operator delete( void *p, std::align_val_t ) noexcept { dlfree( p ); }
void operator delete[]( void *p, std::align_val_t ) noexcept { dlfree( p ); }
void operator delete( void *p, size_t, std::align_val_t ) noexcept { dlfree( p ); }
void operator delete[]( void *p, size_t, std::align_val_t ) noexcept { dlfree( p ); }
