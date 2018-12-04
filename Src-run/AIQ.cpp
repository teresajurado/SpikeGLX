
#include "AIQ.h"
#include "Util.h"


#define SAMPS( arg )    (nchans * (arg))
#define BYTES( arg )    (nchans * sizeof(qint16) * (arg))

/* ---------------------------------------------------------------- */
/* RingWalker ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Utility class assists edge detection around the ring.

class RingWalker {
private:
    const vec_i16   &buf;
    int             bufmax,
                    bufhead,
                    buflen,
                    nchans,
                    chan,
                    icur,
                    len,
                    nrhs;
    quint64         headCt;
public:
    const qint16    *cur;
public:
    RingWalker(
        const vec_i16   &buf,
        int             bufmax,
        int             bufhead,
        int             buflen,
        int             nchans,
        int             chan )
    :   buf(buf), bufmax(bufmax), bufhead(bufhead),
        buflen(buflen), nchans(nchans), chan(chan),
        icur(0) {}

    bool setStart( quint64 fromCt, quint64 endCt );
    bool next();
    quint64 curCt() {return headCt + icur;}
};


bool RingWalker::setStart( quint64 fromCt, quint64 endCt )
{
    headCt = endCt - buflen;

    if( fromCt >= endCt )
        return false;

    if( fromCt < headCt )
        fromCt = headCt;

    int offset  = fromCt - headCt,
        head    = (bufhead + offset) % bufmax;

    len     = buflen - offset;
    nrhs    = std::min( len, bufmax - head );
    cur     = &buf[SAMPS(head) + chan];
    headCt  = fromCt;

    return true;
}


bool RingWalker::next()
{
    if( ++icur >= len )
        return false;

    if( icur != nrhs )
        cur += nchans;
    else
        cur = &buf[chan];

    return true;
}

/* ---------------------------------------------------------------- */
/* RingFltWalker -------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Utility class assists edge detection around the ring;
// filters data via callback usrFlt.

class RingFltWalker {
private:
    const vec_i16       &buf;
    int                 bufmax,
                        bufhead,
                        buflen,
                        nchans,
                        icur,
                        head,
                        len,
                        nrhs;
    quint64             headCt;
    AIQ::T_AIQFilter    &usrFlt;
    int                 nflt,
                        iflt;
public:
    const qint16    *cur;
public:
    RingFltWalker(
        const vec_i16       &buf,
        int                 bufmax,
        int                 bufhead,
        int                 buflen,
        int                 nchans,
        AIQ::T_AIQFilter    &usrFlt )
    :   buf(buf), bufmax(bufmax), bufhead(bufhead),
        buflen(buflen), nchans(nchans), icur(0),
        usrFlt(usrFlt)  {}

    bool setStart( quint64 fromCt, quint64 endCt );
    bool next();
    quint64 curCt() {return headCt + icur;}
private:
    void filter();
};


bool RingFltWalker::setStart( quint64 fromCt, quint64 endCt )
{
    headCt = endCt - buflen;

    if( fromCt >= endCt )
        return false;

    if( fromCt < headCt )
        fromCt = headCt;

    int offset = fromCt - headCt;

    head    = (bufhead + offset) % bufmax;
    len     = buflen - offset;
    nrhs    = std::min( len, bufmax - head );
    headCt  = fromCt;

    filter();

    return true;
}


bool RingFltWalker::next()
{
    if( ++icur >= len )
        return false;

    if( ++iflt >= nflt )
        filter();
    else
        ++cur;

    return true;
}


void RingFltWalker::filter()
{
    const qint16    *src;
    qint16          *dst = &usrFlt.fltbuf[0];

    if( icur < nrhs ) {

        nflt = std::min( usrFlt.nmax, nrhs - icur );
        src  = &buf[SAMPS(head + icur) + usrFlt.chan];
    }
    else {

        nflt = std::min( usrFlt.nmax, len - icur );
        src  = &buf[SAMPS(icur - nrhs) + usrFlt.chan];
    }

    for( int i = 0; i < nflt; ++i, src += nchans )
        *dst++ = *src;

    usrFlt( nflt );

    iflt    = 0;
    cur     = &usrFlt.fltbuf[0];
}

/* ---------------------------------------------------------------- */
/* AIQ ------------------------------------------------------------ */
/* ---------------------------------------------------------------- */

AIQ::AIQ( double srate, int nchans, int capacitySecs )
    :   srate(srate), nchans(nchans), bufmax(capacitySecs * srate),
        tzero(0), endCt(0), bufhead(0), buflen(0)
{
    buf.resize( SAMPS(bufmax) );
}


// Fill with (tLim-t0)*srate zero samples.
//
void AIQ::enqueueZero( double t0, double tLim )
{
    int nCts = (tLim - t0) * srate;

    QMutexLocker    ml( &QMtx );

    endCt += nCts;

    if( nCts >= bufmax ) {
        // Keep only newest bufmax-worth.
        bufhead = 0;
        buflen  = bufmax;
        memset( &buf[0], 0, BYTES(bufmax) );
    }
    else {
        // All new data fit, with some room for old data.
        int newlen  = std::min( buflen + nCts, bufmax ),
            newhead = (bufhead + buflen + nCts - newlen) % bufmax,
            oldtail = (bufhead + buflen) % bufmax,
            ncpy1   = std::min( nCts, bufmax - oldtail );

        memset( &buf[SAMPS(oldtail)], 0, BYTES(ncpy1) );

        if( nCts -= ncpy1 )
            memset( &buf[0], 0, BYTES(nCts) );

        bufhead = newhead;
        buflen  = newlen;
    }
}


void AIQ::enqueue( const qint16 *src, int nCts )
{
    QMutexLocker    ml( &QMtx );

    endCt += nCts;

    if( nCts >= bufmax ) {
        // Keep only newest bufmax-worth.
        bufhead = 0;
        buflen  = bufmax;
        memcpy( &buf[0], &src[SAMPS(nCts-bufmax)], BYTES(bufmax) );
    }
    else {
        // All new data fit, with some room for old data.
        int newlen  = std::min( buflen + nCts, bufmax ),
            newhead = (bufhead + buflen + nCts - newlen) % bufmax,
            oldtail = (bufhead + buflen) % bufmax,
            ncpy1   = std::min( nCts, bufmax - oldtail );

        memcpy( &buf[SAMPS(oldtail)], &src[0], BYTES(ncpy1) );

        if( nCts -= ncpy1 )
            memcpy( &buf[0], &src[SAMPS(ncpy1)], BYTES(nCts) );

        bufhead = newhead;
        buflen  = newlen;
    }
}


void AIQ::enqueueProfile(
    double          &tLock,
    double          &tWork,
    const qint16    *src,
    int             nCts )
{
    double  t, t0 = getTime();

    QMutexLocker    ml( &QMtx );

    t       = getTime();
    tLock   =  t - t0;  // time to get lock

    endCt += nCts;

    if( nCts >= bufmax ) {
        // Keep only newest bufmax-worth.
        bufhead = 0;
        buflen  = bufmax;
        memcpy( &buf[0], &src[SAMPS(nCts-bufmax)], BYTES(bufmax) );
    }
    else {
        // All new data fit, with some room for old data.
        int newlen  = std::min( buflen + nCts, bufmax ),
            newhead = (bufhead + buflen + nCts - newlen) % bufmax,
            oldtail = (bufhead + buflen) % bufmax,
            ncpy1   = std::min( nCts, bufmax - oldtail );

        memcpy( &buf[SAMPS(oldtail)], &src[0], BYTES(ncpy1) );

        if( nCts -= ncpy1 )
            memcpy( &buf[0], &src[SAMPS(ncpy1)], BYTES(nCts) );

        bufhead = newhead;
        buflen  = newlen;
    }

    tWork = getTime() - t;  // time for everything else
}


// Return headCt at front of queue.
//
quint64 AIQ::qHeadCt() const
{
    QMutexLocker    ml( &QMtx );

    return endCt - buflen;
}


// Return count of scans since run start.
//
quint64 AIQ::endCount() const
{
    QMutexLocker    ml( &QMtx );

    return endCt;
}


// Return stream's current wall time.
//
double AIQ::endTime() const
{
    QMutexLocker    ml( &QMtx );

    return tzero + endCt / srate;
}


// Map given time to corresponding count.
// Return {-2=way left, -1=left, 0=inside, 1=right} of stream.
//
int AIQ::mapTime2Ct( quint64 &ct, double t ) const
{
    ct = 0;

    QMutexLocker    ml( &QMtx );

    if( t < tzero || !endCt )
        return -2;

    quint64 C = (t - tzero) * srate;

    if( C >= endCt )
        return 1;

    if( C < endCt - buflen )
        return -1;

    ct = C;

    return 0;
}


// Map given count to corresponding time.
// Return {-2=way left, -1=left, 0=inside, 1=right} of stream.
//
int AIQ::mapCt2Time( double &t, quint64 ct ) const
{
    t = 0;

    QMutexLocker    ml( &QMtx );

    if( !endCt )
        return -2;

    if( ct >= endCt )
        return 1;

    if( ct < endCt - buflen )
        return -1;

    t = tzero + ct / srate;

    return 0;
}


// Copy up to N scans with count >= fromCt.
//
// On entry dest should be cleared and reserved to nominal size.
//
// Return {-1=left of stream, 0=fail, 1=success}.
//
int AIQ::getNScansFromCt(
    vec_i16         &dest,
    quint64         fromCt,
    int             nMax ) const
{
    QMutexLocker    ml( &QMtx );

    quint64 headCt = endCt - buflen;

    if( fromCt >= endCt )
        return 1;

    if( fromCt < headCt )
        return -1;

    int offset  = fromCt - headCt,
        head    = (bufhead + offset) % bufmax,
        len     = buflen - offset;

    nMax = std::min( nMax, len );

// Get up to RHS limit

    int nrhs = std::min( nMax, bufmax - head );

    try {
        dest.insert(
            dest.end(),
            buf.begin() + SAMPS(head),
            buf.begin() + SAMPS(head + nrhs) );
    }
    catch( const std::exception& ) {
        Warning()
            << "AIQ::nScans low mem. SRate " << srate;
        return 0;
    }

// Get any remainder from LHS

    if( nMax -= nrhs ) {

        try {
            dest.insert(
                dest.end(),
                buf.begin(),
                buf.begin() + SAMPS(nMax) );
        }
        catch( const std::exception& ) {
            Warning()
                << "AIQ::nScans low mem. SRate " << srate;
            return 0;
        }
    }

    return 1;
}


// Specialized for mono audio.
// Copy nScans for given channel starting at fromCt.
//
// - Caller must presize dst as nScans*sizeof(qint16).
// - nScans taken as exact: if insufficient scans return -1.
//
// Return headCt or -1 if failure.
//
qint64 AIQ::getNScansFromCtMono(
    qint16          *dst,
    quint64         fromCt,
    int             nScans,
    int             chan ) const
{
    QMutexLocker    ml( &QMtx );

    quint64 headCt = endCt - buflen;

// Off left end?

    if( fromCt < headCt )
        return -1;

// Enough samples available?

    int offset  = fromCt - headCt,
        head    = (bufhead + offset) % bufmax,
        len     = buflen - offset;

    if( len < nScans )
        return -1;

// Get up to RHS limit

    int             nrhs = std::min( nScans, bufmax - head );
    const qint16    *src = &buf[SAMPS(head) + chan];

    for( int i = 0; i < nrhs; ++i, src += nchans )
        dst[i] = *src;

// Get any remainder from LHS

    src = &buf[chan];

    for( int i = nrhs; i < nScans; ++i, src += nchans )
        dst[i] = *src;

    return fromCt;
}


// Specialized for stereo audio.
// Copy nScans for two given channels starting at fromCt.
//
// - Caller must presize dst as 2*nScans*sizeof(qint16).
// - nScans taken as exact: if insufficient scans return -1.
//
// Return headCt or -1 if failure.
//
qint64 AIQ::getNScansFromCtStereo(
    qint16          *dst,
    quint64         fromCt,
    int             nScans,
    int             chan1,
    int             chan2 ) const
{
    QMutexLocker    ml( &QMtx );

    quint64 headCt = endCt - buflen;

// Off left end?

    if( fromCt < headCt )
        return -1;

// Enough samples available?

    int offset  = fromCt - headCt,
        head    = (bufhead + offset) % bufmax,
        len     = buflen - offset;

    if( len < nScans )
        return -1;

// Get up to RHS limit

    int             nrhs = std::min( nScans, bufmax - head );
    const qint16    *src = &buf[SAMPS(head)];

    nrhs   *= 2;
    nScans *= 2;

    for( int i = 0; i < nrhs; i += 2, src += nchans ) {
        dst[i]   = src[chan1];
        dst[i+1] = src[chan2];
    }

// Get any remainder from LHS

    src = &buf[0];

    for( int i = nrhs; i < nScans; i += 2, src += nchans ) {
        dst[i]   = src[chan1];
        dst[i+1] = src[chan2];
    }

    return fromCt;
}


// Specialized for mono audio.
// Copy most recent nScans for given channel.
//
// - Caller must presize dst as nScans*sizeof(qint16).
// - nScans taken as exact: if insufficient scans return -1.
//
// Return headCt or -1 if failure.
//
qint64 AIQ::getNewestNScansMono(
    qint16          *dst,
    int             nScans,
    int             chan ) const
{
    quint64 end = endCount();

    if( end <= quint64(nScans) )
        return -1;

    return
    getNScansFromCtMono( dst, end - nScans, nScans, chan );
}


// Specialized for stereo audio.
// Copy most recent nScans for two given channels.
//
// - Caller must presize dst as 2*nScans*sizeof(qint16).
// - nScans taken as exact: if insufficient scans return -1.
//
// Return headCt or -1 if failure.
//
qint64 AIQ::getNewestNScansStereo(
    qint16          *dst,
    int             nScans,
    int             chan1,
    int             chan2 ) const
{
    quint64 end = endCount();

    if( end <= quint64(nScans) )
        return -1;

    return
    getNScansFromCtStereo( dst, end - nScans, nScans, chan1, chan2 );
}


// Starting from fromCt, scan given chan for rising edge
// (from below to >= T). Including first crossing, require
// signal stays >= T for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findRisingEdge(
    quint64         &outCt,
    quint64         fromCt,
    int             chan,
    qint16          T,
    int             inarow ) const
{
    int     nhi     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingWalker  W( buf, bufmax, bufhead, buflen, nchans, chan );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// -------------------
// Must start on a low
// -------------------

    if( *W.cur >= T ) {

        while( W.next() ) {

            if( *W.cur < T )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( *W.cur >= T ) {

            // Mark edge start
            outCt   = W.curCt();
            nhi     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( *W.cur >= T ) {

                    if( ++nhi >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nhi = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nhi )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


// Acting on filtered data via callback usrFlt...
//
// Starting from fromCt, scan given chan for rising edge
// (from below to >= T). Including first crossing, require
// signal stays >= T for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findFltRisingEdge(
    quint64         &outCt,
    quint64         fromCt,
    qint16          T,
    int             inarow,
    T_AIQFilter     &usrFlt ) const
{
    int     nhi     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingFltWalker  W( buf, bufmax, bufhead, buflen, nchans, usrFlt );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// -------------------
// Must start on a low
// -------------------

    if( *W.cur >= T ) {

        while( W.next() ) {

            if( *W.cur < T )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( *W.cur >= T ) {

            // Mark edge start
            outCt   = W.curCt();
            nhi     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( *W.cur >= T ) {

                    if( ++nhi >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nhi = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nhi )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


// Using specified digital bit within a word...
//
// Starting from fromCt, scan given chan for rising edge
// (0 -> 1). Including first crossing, require
// signal stays high for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findBitRisingEdge(
    quint64         &outCt,
    quint64         fromCt,
    int             chan,
    int             bit,
    int             inarow ) const
{
    int     nhi     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingWalker  W( buf, bufmax, bufhead, buflen, nchans, chan );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// -------------------
// Must start on a low
// -------------------

    if( (*W.cur >> bit) & 1 ) {

        while( W.next() ) {

            if( !((*W.cur >> bit) & 1) )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( (*W.cur >> bit) & 1 ) {

            // Mark edge start
            outCt   = W.curCt();
            nhi     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( (*W.cur >> bit) & 1 ) {

                    if( ++nhi >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nhi = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nhi )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


// Starting from fromCt, scan given chan for falling edge
// (from above to < T). Including first crossing, require
// signal stays < T for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findFallingEdge(
    quint64         &outCt,
    quint64         fromCt,
    int             chan,
    qint16          T,
    int             inarow ) const
{
    int     nlo     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingWalker  W( buf, bufmax, bufhead, buflen, nchans, chan );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// --------------------
// Must start on a high
// --------------------

    if( *W.cur < T ) {

        while( W.next() ) {

            if( *W.cur >= T )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( *W.cur < T ) {

            // Mark edge start
            outCt   = W.curCt();
            nlo     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( *W.cur < T ) {

                    if( ++nlo >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nlo = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nlo )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


// Acting on filtered data via callback usrFlt...
//
// Starting from fromCt, scan given chan for falling edge
// (from above to < T). Including first crossing, require
// signal stays < T for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findFltFallingEdge(
    quint64         &outCt,
    quint64         fromCt,
    qint16          T,
    int             inarow,
    T_AIQFilter     &usrFlt ) const
{
    int     nlo     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingFltWalker  W( buf, bufmax, bufhead, buflen, nchans, usrFlt );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// --------------------
// Must start on a high
// --------------------

    if( *W.cur < T ) {

        while( W.next() ) {

            if( *W.cur >= T )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( *W.cur < T ) {

            // Mark edge start
            outCt   = W.curCt();
            nlo     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( *W.cur < T ) {

                    if( ++nlo >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nlo = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nlo )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


// Using specified digital bit within a word...
//
// Starting from fromCt, scan given chan for falling edge
// (1 -> 0). Including first crossing, require
// signal stays low for at least inarow counts.
//
// Return:
// false = no edge; resume looking from outCt.
// true  = edge @ outCt.
//
bool AIQ::findBitFallingEdge(
    quint64         &outCt,
    quint64         fromCt,
    int             chan,
    int             bit,
    int             inarow ) const
{
    int     nlo     = 0;
    bool    found   = false;

    outCt = fromCt;

    QMutexLocker    ml( &QMtx );

    RingWalker  W( buf, bufmax, bufhead, buflen, nchans, chan );

    if( !W.setStart( fromCt, endCt ) )
        return false;

// --------------------
// Must start on a high
// --------------------

    if( !((*W.cur >> bit) & 1) ) {

        while( W.next() ) {

            if( (*W.cur >> bit) & 1 )
                goto seek_edge;
        }

        goto exit;
    }

// -------------------
// Seek edge candidate
// -------------------

seek_edge:
    while( W.next() ) {

        if( !((*W.cur >> bit) & 1) ) {

            // Mark edge start
            outCt   = W.curCt();
            nlo     = 1;

            if( inarow == 1 ) {
                found = true;
                goto exit;
            }

            // Check extended run length
            while( W.next() ) {

                if( !((*W.cur >> bit) & 1) ) {

                    if( ++nlo >= inarow ) {
                        found = true;
                        goto exit;
                    }
                }
                else {
                    nlo = 0;
                    break;
                }
            }
        }
    }

// ----
// Exit
// ----

exit:
    if( !found ) {

        if( nlo )
            --outCt;    // review last candidate again
        else
            outCt = endCt;
    }

    return found;
}


