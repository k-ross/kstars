/***************************************************************************
                          skymesh.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-03
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMESH_H
#define SKYMESH_H


#include <QHash>
#include <QList>
#include <QObject>

#include <QPainter>

#include "ksnumbers.h"

#include "../htmesh/HTMesh.h"
#include "../htmesh/MeshIterator.h"
#include "../htmesh/MeshBuffer.h"

#include "typedef.h"

class KSNumbers;
class StarObject;

class SkyPoint;
class QPolygonF;

class KStarsData;
class QPainter;

// These enums control the trixel storage.  Separate buffers are available for
// indexing and intersecting.  Currently only one buffer is required.  Multiple
// buffers could be used for intermingling drawing and searching or to have a
// non-precessed aperture for objects that don't precess.  If a buffer number
// is greater than or equal to NUM_BUF a brief error message will be printed
// and then KStars will crash.  This is a feature, not a bug.

enum MeshBufNum_t {
    DRAW_BUF        = 0,
    NO_PRECESS_BUF  = 1,
    OBJ_NEAREST_BUF = 2,
    IN_CONSTELL_BUF = 3,
    NUM_MESH_BUF
};

/*@class SkyMesh
 * Provides an interface to the Hierarchical Triangular Mesh (HTM) library
 * written by A. Szalay, John Doug Reynolds, Jim Gray, and Peter Z. Kunszt.
 *
 * HTM divides the celestial sphere up into little triangles (trixels) and
 * gives each triangle a unique integer id.  We index objects by putting a
 * pointer to the object in a data structure accessible by the id number, for
 * example QList<QList<StarObject*> or QHash<int, QList<LineListComponent*>.
 *
 * Use aperture(centerPoint, radius) to get a list of trixels visible in the
 * circle.  Then use a MeshIterator to iterate over the indices of the visible
 * trixels.  Look up each index in the top level QHash or QList data structure
 * and the union of all the elements of the QList's returned will contain all
 * the objects visible in the circle in the sky specfied in the aperture()
 * call.
 *
 * The drawID is used for extended objects that may be covered by more than
 * one trixel.  Every time aperture() is called, drawID is incremented by one.
 * Extended objects should each contain their own drawID.  If an object's
 * drawID is the same as the current drawID then it shouldn't be drawn again
 * because it was already drawn in this draw cycle.  If the drawID is
 * different then it should be drawn and then the drawID should be set to the
 * current drawID() so it doesn't get drawn again this cycle.
 *
 * The list of visible trixels found from an aperature() call or one of the
 * primitive index() by creating a new MeshIterator instance:
 *
 *      MeshIterator( HTMesh *mesh )
 *
 * The MeshIterator has its own bool hasNext(), int next(), and int size()
 * methods for iterating through the integer indices of the found trixels or
 * for just getting the total number of found trixels.
 */

class SkyMesh : public HTMesh
{
protected:

    SkyMesh( KStarsData* data, int level );
    SkyMesh( SkyMesh& skyMesh );

public:
    /* @short creates the single instance of SkyMesh.  The level indicates
     * how fine a mesh we will use. The number of triangles (trixels) in the
     * mesh will be 8 * 4^level so a mesh of level 5 will have 8 * 4^5 = 8 *
     * 2^10 = 8192 trixels.  The size of the triangles are roughly pi / *
     * 2^(level + 1) so a level 5 mesh will have triagles size roughly of
     * .05 radians or 2.8 degrees.
           */
    static SkyMesh* Create( KStarsData* data, int level );

    /* @short returns the single instance of SkyMesh or null if it has not
     * yet been created.
     */
    static SkyMesh* Instance();

    /**@short finds the set of trixels that cover the circular aperture
     * specified after first performing a reverse precession correction on
     * the center so we don't have to re-index objects simply due to
     * precession.  The drawID also gets incremented which is useful for
     * drawing extended objects.  Typically a safety factor of about one
     * degree is added to the radius to account for proper motion,
     * refraction and other imperfections.
     */
    void aperture( SkyPoint *center, double radius, MeshBufNum_t bufNum=DRAW_BUF );

    /* @short returns the index of the trixel containing p.
     */
    Trixel index( SkyPoint *p );


    //----- Stars and CLines -----

    // The follow four routines are used for indexing stars and CLines.
    // They differ from the normal routines because they take proper motion
    // into account while the normal routines always index the J2000
    // position.
    //
    // Since the proper motion depends on time, it is essential to call
    // setKSNumbers to set the time before doing the indexing.  The default
    // value is J2000.

    /* @short sets the time for indexing StarObjects and CLines.
     */
    void setKSNumbers( KSNumbers* num ) { m_KSNumbers = KSNumbers( *num ); }

    /* @short returns the trixel that contains the star at the set
     * time with proper motion taken into account but not precession.
     * This is a feature not a bug.
     */
    Trixel indexStar( StarObject *star );

    /* @short fills the default buffer with all the trixels needed to cover
     * the line connecting the two stars.
     */
    void indexStar( StarObject* star1, StarObject* star2 );

    /* @short Fills a hash with all the trixels needed to cover all the line
     * segments in the SkyList where each SkyPoint is assumed to be a star
     * and proper motion is taken into account.  Used only by
     * ConstellationsLines.
     */
    const IndexHash& indexStarLine( SkyList* points );


    //----- Here come index routines for various shapes -----

    //--- First the QPointF routines ---

    /* @short finds the indices of the trixels that cover the triangle
     * specified by the three QPointF's.
     */
    void index( const QPointF &p1, const QPointF &p2, const QPointF &p3 );

    /* @short Finds the trixels needed to cover the quadrilateral specifed
     * by the four QPointF's.
     */
    void index( const QPointF &p1, const QPointF &p2, const QPointF &p3,
                const QPointF &p4 );


    //--- Now the SkyPoint Routines ---

    /* @short finds the indices of the trixels covering the circle specified
     * by center and radius.
     */
    void index( SkyPoint *center, double radius, MeshBufNum_t bufNum=DRAW_BUF );

    /* @short finds the indices of the trixels covering the line segment
     * connecting p1 and p2.
     */
    void index( SkyPoint* p1, SkyPoint* p2 );

    /* @short finds the indices of the trixels covering the triangle
     * specified by vertices: p1, p2, and p3.
     */
    void index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3 );

    /* @short finds the indices of the trixels covering the quadralateral
     * specified by the vertices: p1, p2, p3, and p4.
     */
    void index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3, SkyPoint* p4 );


    //----- IndexHash Routines -----

    // The follow routines are used to index SkyList data structures.  They
    // fill a QHash with the indices of the trixels that cover the data
    // structure.  These are all used as callbacks in the LineListIndex
    // subclasses so they can use the same indexing code to index differnt
    // data structures.  See also indexStarLine() above.

    /* @short fills a QHash with the trixel indices needed to cover all the
     * line segments specified in the QVector<SkyPoints*> points.
     *
     * @param points the points of the line segment.  @debug causes extra
     * info to be printed.
     */
    const IndexHash& indexLine( SkyList* points );

    /* @short as above but allows for skipping the indexing of some of
     * the points.
     *
     * @param points the line segment to be indexed.
     * @param indexHash the hash to return the indices in.
     * @param skip a hash indicating which points to skip
     * @param debug optional flag to have the routine print out
     * debugging info.
     */
    const IndexHash& indexLine( SkyList* points, IndexHash* skip );

    /* @short fills a QHash with the trixel indices needed to cover the
     * polygon specified in the QList<SkyPoints*> points.  There is no
     * version with a skip parameter because it does not make sense to
     * skip some of the edges of a filled polygon.
     *
     * @param points the points of the line segment.
     * @debug causes extra info to be printed.
     */
    const IndexHash& indexPoly( SkyList* points );

    /* @short does the same as above but with a QPolygonF as the
     * container for the points.
     */
    const IndexHash& indexPoly( const QPolygonF* points );

    /* @short Returns the debug level.  This is used as a global debug level
     * for LineListIndex and its subclasses.
     */
    int debug() { return m_debug; }

    /* @short Sets the debug level.
     */
    void debug( int debug ) { m_debug = debug; }

    /* @return the current drawID which gets incremented each time aperture()
     * is called.
     */
    DrawID drawID( ) { return m_drawID; }

    /* @short increments the drawID and returns the new value.  This is
     * useful when you want to use the drawID to ensure you are not
     * repeating yourself when iterating over the elements of an IndexHash.
     * It is currently used in LineListIndex::reindex().
     */
    int incDrawID() { return ++m_drawID; }

    /* @short Draws the outline of all the trixels in the specified buffer.
     * This was very useful during debugging.  I don't precess the points
     * because I mainly use it with the IN_CONSTELL_BUF which is not
     * precessed.  We will probably have buffers serve double and triple
     * duty in the production code to save space in which case this function
     * may become less useful.
     */
    void draw( QPainter& psky, MeshBufNum_t
               bufNum=DRAW_BUF );

    /* @short Returns true of the most recent aperture() call produced
     * fewer than m_zoomFraction of the total available trixels.
     */
    bool isZoomedIn(int percent=0 );

    bool inDraw() { return m_inDraw; }
    void inDraw( bool inDraw ) { m_inDraw = inDraw; }

private:
    DrawID m_drawID;
    int    errLimit;
    int    m_debug;

    IndexHash   indexHash;
    KStarsData* m_data;
    KSNumbers   m_KSNumbers;
    int         m_zoomedInPercent;

    bool        m_inDraw;
    static SkyMesh* pinstance;
};

#endif
