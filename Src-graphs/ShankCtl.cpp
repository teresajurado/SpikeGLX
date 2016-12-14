
#include "ui_ShankWindow.h"

#include "Util.h"
#include "MainApp.h"
#include "ShankCtl.h"
#include "DAQ.h"
#include "ShankMap.h"
#include "Biquad.h"
#include "SignalBlocker.h"

#include <QCloseEvent>

/* ---------------------------------------------------------------- */
/* class Tally ---------------------------------------------------- */
/* ---------------------------------------------------------------- */

void ShankCtl::Tally::init( double sUpdt, bool bImec )
{
    isImec = bImec;

    if( bImec )
        nPads = p.im.imCumTypCnt[CimCfg::imSumAP];
    else
        nPads = p.ni.niCumTypCnt[CniCfg::niSumNeural];

    updtChanged( sUpdt );
}


#define SEC_PER_FETCH   0.1

void ShankCtl::Tally::updtChanged( double s )
{
    chunksReqd = qMax( 1, int(s / SEC_PER_FETCH) );
    zeroData();
}


void ShankCtl::Tally::zeroData()
{
    vmin.fill(  99000, nPads );
    vmax.fill( -99000, nPads );
    sums.fill( 0, nPads );
    sumSamps    = 0;
    chunksDone  = 0;
}


bool ShankCtl::Tally::countSpikes(
    const short *data,
    int         ntpts,
    int         nchans,
    int         c0,
    int         cLim,
    int         thresh )
{
    if( !ntpts )
        return false;

    sumSamps += ntpts;

    for( int c = c0; c < cLim; ++c ) {

        const short *d      = &data[c],
                    *dlim   = &data[c + ntpts*nchans];
        int T       = (isImec ?
                        p.im.vToInt10( thresh*1e-6, c - c0 ) :
                        p.ni.vToInt16( thresh*1e-6, c - c0 ));
        int hiCnt   = -1;
    }

    bool    done = ++chunksDone >= chunksReqd;

    if( done ) {

        double  count2Rate = (isImec ? p.im.srate : p.ni.srate) / sumSamps;

        for( int i = 0; i < nPads; ++i )
            sums[i] *= count2Rate;
    }

    return done;
}


bool ShankCtl::Tally::accumPkPk(
    const short *data,
    int         ntpts,
    int         nchans,
    int         c0,
    int         cLim )
{
    if( !ntpts )
        return false;

    for( int it = 0; it < ntpts; ++it, data += nchans ) {

        for( int c = c0; c < cLim; ++c ) {

            int v = data[c],
                i = c - c0;

            if( v < vmin[i] )
                vmin[i] = v;

            if( v > vmax[i] )
                vmax[i] = v;
        }
    }

    bool    done = ++chunksDone >= chunksReqd;

    if( done ) {

        for( int i = 0; i < nPads; ++i )
            sums[i] = vmax[i] - vmin[i];
    }

    return done;
}

/* ---------------------------------------------------------------- */
/* ShankCtl ------------------------------------------------------- */
/* ---------------------------------------------------------------- */

ShankCtl::ShankCtl( const DAQ::Params &p, QWidget *parent )
    :   QWidget(parent), p(p), tly(p), hipass(0), lopass(0)
{
}


ShankCtl::~ShankCtl()
{
    drawMtx.lock();

        if( hipass ) {
            delete hipass;
            hipass = 0;
        }

        if( lopass ) {
            delete lopass;
            lopass = 0;
        }

    drawMtx.unlock();

    if( scUI ) {
        delete scUI;
        scUI = 0;
    }

    emit closed( this );
}

/* ---------------------------------------------------------------- */
/* Public --------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void ShankCtl::showDialog()
{
    show();
    scUI->scroll->scrollToSelected();
    mainApp()->modelessOpened( this );
}


void ShankCtl::update()
{
    scUI->scroll->theV->updateNow();
}


void ShankCtl::selChan( int ic, const QString &name )
{
    const ShankMap  *M = scUI->scroll->theV->getSmap();

    if( M && ic < M->e.size() ) {

        scUI->scroll->theV->setSel( ic );
        scUI->chanBut->setText( name );
    }
}


void ShankCtl::layoutChanged()
{
    scUI->scroll->theV->setShankMap( scUI->scroll->theV->getSmap() );
}

/* ---------------------------------------------------------------- */
/* Slots ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void ShankCtl::chanButClicked()
{
    scUI->scroll->scrollToSelected();
}


void ShankCtl::ypixChanged( int y )
{
    set.yPix = y;
    scUI->scroll->setRowPix( y );
    saveSettings();
}


void ShankCtl::whatChanged( int i )
{
    drawMtx.lock();
        set.what = i;
        SignalBlocker   b0(scUI->rngSB);
        scUI->TSB->setEnabled( !i );
        scUI->rngSB->setValue( set.rng[i] );
        updateFilter( false );
        tly.zeroData();
        scUI->scroll->theV->colorPads( tly.sums, 1e99 );
        QMetaObject::invokeMethod(
            scUI->scroll->theV,
            "updateNow",
            Qt::QueuedConnection );
    drawMtx.unlock();
    saveSettings();
}


void ShankCtl::threshChanged( int t )
{
    drawMtx.lock();
        set.thresh = -t;
        tly.zeroData();
    drawMtx.unlock();
    saveSettings();
}


void ShankCtl::rangeChanged( int r )
{
    drawMtx.lock();
        set.rng[set.what] = r;
        tly.zeroData();
    drawMtx.unlock();
    saveSettings();
}


void ShankCtl::updtChanged( double s )
{
    drawMtx.lock();
        set.updtSecs = s;
        tly.updtChanged( s );
    drawMtx.unlock();
    saveSettings();
}

/* ---------------------------------------------------------------- */
/* Protected ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

void ShankCtl::baseInit()
{
    loadSettings();

    scUI = new Ui::ShankWindow;
    scUI->setupUi( this );

    scUI->scroll->theV->setRowPix( set.yPix );
    scUI->ypixSB->setValue( set.yPix );
    scUI->whatCB->setCurrentIndex( set.what );
    scUI->TSB->setValue( -set.thresh );
    scUI->TSB->setEnabled( set.what == 0 );
    scUI->rngSB->setValue( set.rng[set.what] );
    scUI->updtSB->setValue( set.updtSecs );

    ConnectUI( scUI->scroll->theV, SIGNAL(cursorOver(int,bool)), this, SLOT(cursorOver(int,bool)) );
    ConnectUI( scUI->scroll->theV, SIGNAL(lbutClicked(int,bool)), this, SLOT(lbutClicked(int,bool)) );
    ConnectUI( scUI->ypixSB, SIGNAL(valueChanged(int)), this, SLOT(ypixChanged(int)) );
    ConnectUI( scUI->whatCB, SIGNAL(currentIndexChanged(int)), this, SLOT(whatChanged(int)) );
    ConnectUI( scUI->TSB, SIGNAL(valueChanged(int)), this, SLOT(threshChanged(int)) );
    ConnectUI( scUI->rngSB, SIGNAL(valueChanged(int)), this, SLOT(rangeChanged(int)) );
    ConnectUI( scUI->updtSB, SIGNAL(valueChanged(double)), this, SLOT(updtChanged(double)) );
    ConnectUI( scUI->chanBut, SIGNAL(clicked()), this, SLOT(chanButClicked()) );

    updateFilter( true );

    setAttribute( Qt::WA_DeleteOnClose, false );
}


void ShankCtl::zeroFilterTransient( short *data, int ntpts, int nchans )
{
    if( nzero > 0 ) {

        // overwrite with zeros

        if( ntpts > nzero )
            ntpts = nzero;

        memset( data, 0, ntpts*nchans*sizeof(qint16) );
        nzero -= ntpts;
    }
}


void ShankCtl::dcAve(
    QVector<int>    &ave,
    short           *data,
    int             maxInt,
    int             ntpts,
    int             nchans,
    int             c0,
    int             cLim )

{
    int             nC = cLim - c0;
    QVector<float>  sum( nC, 0.0F );

    for( int it = 0; it < ntpts; ++it, data += nchans ) {

        for( int c = c0; c < cLim; ++c )
            sum[c-c0] += data[c];
    }

    ave.resize( nC );

    for( int c = 0; c < nC; ++c ) {

        int a = sum[c]/ntpts;

        if( a < -maxInt )
            a = -maxInt;
        else if( a >= maxInt )
            a = maxInt - 1;

        ave[c] = a;
    }
}


void ShankCtl::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Escape ) {

        close();
        e->accept();
    }
    else
        QWidget::keyPressEvent( e );
}


void ShankCtl::closeEvent( QCloseEvent *e )
{
    QWidget::closeEvent( e );

    if( e->isAccepted() ) {

        // reset for next showing of window
        nzero = BIQUAD_TRANS_WIDE;

        emit closed( this );
    }
}

/* ---------------------------------------------------------------- */
/* Private -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

