//------------------------------------------------------------------------------
/// \file
/// \ingroup interpolate_detail
/// \copyright (C) Copyright Aquaveo 2018. Distributed under the xmsng
///  Software License, Version 1.0. (See accompanying file
///  LICENSE_1_0.txt or copy at http://www.aquaveo.com/xmsng/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

//----- Included files ---------------------------------------------------------

// 1. Precompiled header

// 2. My header
#include <xmsinterp/interpolate/detail/AnisotropicInterpolator.h>

// 3. Standard Library Headers
#include <cmath>
#include <functional>
#include <limits>

// 4. External Library Headers

// 5. Shared Headers
#include <xmscore/misc/XmError.h>
#include <xmscore/stl/vector.h>
#include <xmsinterp/geometry/geoms.h>

// 6. Non-shared Headers

//----- Forward declarations ---------------------------------------------------

//----- External globals -------------------------------------------------------

//----- Namespace declaration --------------------------------------------------
namespace xms
{
/// \brief Slope value indicating that a line is vertical.
const double kVERTICAL = std::numeric_limits<double>::quiet_NaN();

//------------------------------------------------------------------------------
/// \brief Constructor
/// \param[in] a_slope The slope of the line. Use kVERTICAL for vertical lines.
/// \param[in] a_intercept The x intercept if vertical; else, the y intercept.
//------------------------------------------------------------------------------
LineParameters::LineParameters(double a_slope, double a_intercept)
: m_slope(a_slope)
, m_intercept(a_intercept)
{
} // LineParameters::LineParameters

//------------------------------------------------------------------------------
/// \brief Return the parameters for a line normal to this one through a point.
/// \param[in] a_p The point through which the normal line will contain.
//------------------------------------------------------------------------------
LineParameters LineParameters::NormalThrough(const Pt3d& a_p) const
// Get parameters of a line perpendicular to this through point p.
{
  if (std::isnan(m_slope))
  {
    return LineParameters(0.0, a_p.y);
  }
  if (m_slope == 0.0)
  {
    return LineParameters(kVERTICAL, a_p.x);
  }
  double slope = -1.0 / m_slope;
  return LineParameters(slope, a_p.y - slope * a_p.x);
} // LineParameters::NormalThrough

//------------------------------------------------------------------------------
/// \brief Compute the intersection point of this line with another.
/// \param[in] a_other The other line's definition.
/// \param[out] a_p The intersection point (if any)
/// \return True if there is an intersection. False if the lines are parallel.
//------------------------------------------------------------------------------
bool LineParameters::Intersection(const LineParameters& a_other, Pt3d& a_p) const
{
  if (m_slope == a_other.m_slope ||
      (std::isnan(m_slope) && std::isnan(a_other.m_slope))) // Parallel Lines
  {
    return false;
  }

  double a(m_slope), b(m_intercept);
  double c(a_other.m_slope), d(a_other.m_intercept);
  if (std::isnan(m_slope))
  {
    a_p.x = b;
    a_p.y = c * b + d;
    return true;
  }
  a_p.x = std::isnan(a_other.m_slope) ? d : ((d - b) / (a - c));
  a_p.y = a * a_p.x + b;
  return true;
} // LineParameters::Intersection

//------------------------------------------------------------------------------
/// \brief Constructor
//------------------------------------------------------------------------------
AnisotropicInterpolator::AnisotropicInterpolator()
: m_centerLinePts()
, m_accLengths()
, m_lineParams()
, m_SNPoints()
{
} // AnisotropicInterpolator::AnisotropicInterpolator
//------------------------------------------------------------------------------
/// \brief Set the centerline points to parameterize to.
/// \param[in] a_centerLinePts The centerline points.
/// \param[in] a_interpolationPts The points to be interpolated from (typically
/// cross sections).
/// \param[in] a_pickClosest True to only transform interpolation points to the
/// closest x station on the centerline.  If false, interpolation points will
/// transform to every segment of the centerline where a normal to the segment
/// through the point is between the points defining the segment.
//------------------------------------------------------------------------------
void AnisotropicInterpolator::SetPoints(const VecPt3d& a_centerLinePts,
                                        const VecPt3d& a_interpolationPts,
                                        bool a_pickClosest)
{
  double total = 0.0;

  size_t points_sz = a_centerLinePts.size();
  m_centerLinePts.reserve(points_sz);
  m_accLengths.reserve(points_sz);
  m_lineParams.reserve(points_sz - 1);
  m_centerLinePts.push_back(a_centerLinePts[0]);
  m_accLengths.push_back(0.0);
  for (size_t i = 1; i < points_sz; ++i)
  {
    const Pt3d& p0 = a_centerLinePts[i - 1];
    const Pt3d& p1 = a_centerLinePts[i];
    if (p0.x == p1.x && p0.y == p1.y)
      continue;
    total += Distance(p0, p1);
    m_accLengths.push_back(total);
    m_lineParams.push_back(GetLineParameters(p0, p1));
    m_centerLinePts.push_back(p1);
  }
  
  m_SNPoints = ComputeInterpolationPoints(a_interpolationPts, a_pickClosest);
} // AnisotropicInterpolator::SetCenterlinePoints
//------------------------------------------------------------------------------
/// \brief Transforms a set of points from normal (x, y, z) coordinates to
/// (s, n, z) coordinates. Note: The z coordinate is left unchanged.
/// \param[in] a_points The points to transform. (Typically cross sections.)
/// \param[in] a_pickClosest True to produce only one (s, n, value) for each
/// input point.
/// \return a vector of points in (s, n, z) space. See TransformPoint for a
/// definition of (s, n, z).
//------------------------------------------------------------------------------
VecPt3d AnisotropicInterpolator::ComputeInterpolationPoints(
        const VecPt3d& a_points, bool a_pickClosest)
{
  VecPt3d results;
  for (auto& p : a_points)
  {
    VecPt3d tranformedPts = TransformPoint(p, a_pickClosest);
    results.insert(results.end(), tranformedPts.begin(), tranformedPts.end());
  }
  return results;
} // AnisotropicInterpolator::ComputeInterpolationPoints
//------------------------------------------------------------------------------
/// \brief Get the transformed interpolation points in (s, n, z) space.
/// \return a vector of points in (s, n, value) space.
//------------------------------------------------------------------------------
const VecPt3d &AnisotropicInterpolator::GetInterpolationPoints() const
{
  return m_SNPoints;
} // AnisotropicInterpolator::GetSNVInterpolationPoints
//------------------------------------------------------------------------------
/// \brief Interpolate the z coordinate of the interpolation points to point
///        a_target.
/// \param[in] a_target The (x, y, z) point whose z coordinate is to be
///                     interpolated.
/// \param[in] a_interpolationPoints The points to interpolate from
///                     (presumably in [s, n, v] space).
/// \param[in] a_xScale The scale factor to apply to the x coordinates when
///                     computing anisotropic distance.
/// \param[in] a_IDWPower The power to apply to the inverse distance weighting.
/// \param[in] a_tol If the distance squared between a_target and a point in
///                  a_points is less than or equal to this tolerance, just
///                  return the z coordinate of that interpolation point.
/// \return A weighted interpolation of the z-coordinate values of a_points
///         based on the inverse of the distance from each point in a_points.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::InterpolatePoint(const Pt3d& a_target,
                                                 const VecPt3d& a_interpolationPoints,
                                                 double a_xScale,
                                                 double a_IDWPower,
                                                 double a_tol)
{
  VecPt3d snvs = TransformPoint(a_target, true);
  return CalculateIDW(a_xScale, a_interpolationPoints, snvs[0], a_IDWPower, a_tol);
} // AnisotropicInterpolator::InterpolatePoint
//------------------------------------------------------------------------------
/// \brief Compute the distance between two points.
/// \param[in] a_p0 The first point.
/// \param[in] a_p1 The second point.
/// \return The distance between the points.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::Distance(const Pt3d& a_p0, const Pt3d& a_p1) const
{
  double dx(a_p0.x - a_p1.x);
  double dy(a_p0.y - a_p1.y);
  double distanceSquared(dx * dx + dy * dy);
  return sqrt(distanceSquared);
} // AnisotropicInterpolator::Distance
//------------------------------------------------------------------------------
/// \brief Compute the distance squared between two points, after a scale factor
///        is applied to their x coordinates.
/// \param[in] a_p0 The first point.
/// \param[in] a_p1 The second point.
/// \param[in] a_xScale The scale factor to apply to the x coordinate.
/// \return The anisotropic distance between the points.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::AnisotropicDistanceSquared(const Pt3d& a_p0,
                                                           const Pt3d& a_p1,
                                                           double a_xScale)
{
  double dx = a_xScale * (a_p0.x - a_p1.x);
  double dy = a_p0.y - a_p1.y;
  return (dx * dx) + (dy * dy);
} // AnisotropicInterpolator::AnisotropicDistanceSquared
//------------------------------------------------------------------------------
/// \brief Calculates the Inverse Distance Weighted interpolation of a set of
///        points at a target point.
/// \param[in] a_xScale The scale factor to apply to the x coordinates when
///                      computing anisotropic distance.
/// \param[in] a_points The points to interpolate from (presumably in [s, n, z]
/// space).
/// \param[in] a_target The point in (s,n,?) whose z coordinate is to be
/// interpolated.
/// \param[in] a_power The exponent to apply to the inverse distance weighting.
/// \param[in] a_tol If the distance squared between a_target and a point in
///                   a_points is less than or equal to this tolerance, just
///                   return the z coordinate of that point.
/// \return A weighted interpolation of the z-coordinate values of a_points
///         based on the inverse of the distance from each point in a_points.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::CalculateIDW(double a_xScale,
                                             const VecPt3d& a_points,
                                             const Pt3d& a_target,
                                             double a_power,
                                             double a_tol)
{
  double numerator(0.0);
  double denominator(0.0);
  double power(double(a_power) * 0.5);
  std::function<double(double)> powerFunc;
  if (a_power == 2)
    powerFunc = [](double x) -> double { return x; };
  else
    powerFunc = [=](double x) -> double { return pow(x, power); };
  for (auto& p : a_points)
  {
    double distanceSquared(AnisotropicDistanceSquared(p, a_target, a_xScale));
    double value = p.z;
    if (distanceSquared <= a_tol)
    {
      return value;
    }
    double weight(1.0 / powerFunc(distanceSquared));
    numerator += value * weight;
    denominator += weight;
  }
  return numerator / denominator;
} // AnisotropicInterpolator::CalculateIDW
//------------------------------------------------------------------------------
/// \brief Compute the 2d cross product of the vectors from a_p to the 1st and
///        last points in a segment of m_centerLinePts. The cross product will
///        be positive if a_p is to the left of the vector from first to last
///        point in the segment, negative if to the right, and 0.0 if on the
///        line segment.
/// \param[in] a_segmentIndex The index of the segment of m_centerlinePts;
/// \param[in] a_p A point in (x, y) space (typically in a cross section).
/// \return The 2d cross product.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::CrossProduct(size_t a_segmentIndex, const Pt3d& a_p) const
{
  const Pt3d& pa = a_p;
  const Pt3d& pb = m_centerLinePts[a_segmentIndex];
  const Pt3d& pc = m_centerLinePts[a_segmentIndex + 1];
  double bcx = pc.x - pb.x;
  double bcy = pc.y - pb.y;
  double bax = pa.x - pb.x;
  double bay = pa.y - pb.y;
  return gmCross2D(bcx, bcy, bax, bay);
  //return (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
} // AnisotropicInterpolator::CrossProduct
//------------------------------------------------------------------------------
/// \brief Compute the 2d slope and intercept for a line through two points.
/// \param[in] a_p0 The first point.
/// \param[in] a_p1 The second point.
/// \return The LineParameters for the 2d line between the points.
//------------------------------------------------------------------------------
LineParameters AnisotropicInterpolator::GetLineParameters(const Pt3d& a_p0, const Pt3d& a_p1) const
{
  if (a_p0.x == a_p1.x) // Vertical Line
  {
    return LineParameters(kVERTICAL, a_p0.x);
  }
  double slope = (a_p1.y - a_p0.y) / (a_p1.x - a_p0.x);
  double intercept(a_p0.y - slope * a_p0.x);
  return LineParameters(slope, intercept);
} // AnisotropicInterpolator::GetLineParameters
//------------------------------------------------------------------------------
/// \brief Compute the parameter for a point in a segment of m_centerLinePts.
/// The parameter will be between 0 and 1 if the normal to the segment through
/// the point is between the segment end points. It will be < 0 if the point
/// projects onto the segment before the first point and > 1 if it projects
/// onto the line after the second point.
/// \param[in] a_segmentIndex The index of the segment of m_centerLinePts.
/// \param[in] a_p A point whose parameter in that segment is wanted.
/// \return The parametric position of the point within the segment.
//------------------------------------------------------------------------------
double AnisotropicInterpolator::GetParameterInSegment(size_t a_segmentIndex, const Pt3d& a_p) const
{
  const Pt3d& pi = m_centerLinePts[a_segmentIndex];
  const Pt3d& pf = m_centerLinePts[a_segmentIndex + 1];
  if (pi.x == pf.x) // The segment is vertical
  {
    XM_ASSERT(pi.y != pf.y); // It cannot be zero length!
    return (a_p.y - pi.y) / (pf.y - pi.y);
  }
  return (a_p.x - pi.x) / (pf.x - pi.x);
} // AnisotropicInterpolator::GetParameterInSegment
//------------------------------------------------------------------------------
/// \brief Get the intersecting point and its parameter of a line through point
///        a_p normal to a segment of m_centerLinePts.
/// \param[in] a_segmentIndex The index of a segment of m_centerLinePts.
/// \param[in] a_p A point.
/// \param[out] a_intersection The intersection of a line normal to the segment
/// through a_p.
/// \param[out] a_param The parameter of the intersection in a range of [0..1]
/// if the intersection is within the segment.
/// \return true if there is an intersection; false otherwise.
//------------------------------------------------------------------------------
bool AnisotropicInterpolator::GetIntersectionOfSegmentWithPoint(size_t a_segmentIndex,
                                                                const Pt3d& a_p,
                                                                Pt3d& a_intersection,
                                                                double& a_param) const
{
  const LineParameters& line = m_lineParams[a_segmentIndex];
  const LineParameters normal = line.NormalThrough(a_p);
  if (line.Intersection(normal, a_intersection))
  {
    a_param = GetParameterInSegment(a_segmentIndex, a_intersection);
    return true;
  }
  return false;
} // AnisotropicInterpolator::GetIntersectionOfSegmentWithPoint
//------------------------------------------------------------------------------
/// \brief Append the transform (if any) of a point to a centerline segment
/// \param[in] a_segmentIndex The segment of m_centerLinePts.
/// \param[in] a_p The (x, y, z) point to transform.
/// \param[in] a_lastParam The parameter of a_p for the previous segment
///                         (a_segmentIndex - 1).
/// \param[out] a_results: A vector a SNResult where the transform will be
///                        collected.
//------------------------------------------------------------------------------
void AnisotropicInterpolator::GetNormalPointParameter(size_t a_segmentIndex,
                                                      const Pt3d& a_p,
                                                      double a_lastParam,
                                                      VecSNResult& a_results) const
{
  Pt3d intersection;
  double param;
  double cross = CrossProduct(a_segmentIndex, a_p);
  if (GetIntersectionOfSegmentWithPoint(a_segmentIndex, a_p, intersection, param))
  {
    if (param >= 0.0)
    {
      if (param <= 1.0)
      {
        a_results.push_back(SNResult(a_segmentIndex, param, cross, intersection));
      }
    }
    else if (a_lastParam > 1.0)
    {
      const Pt3d& pa = m_centerLinePts[a_segmentIndex];
      a_results.push_back(SNResult(a_segmentIndex, 0.0, cross, pa));
      param = 0.0;
    }
    a_lastParam = param;
  }
} // AnisotropicInterpolator::GetNormalPointParameter
//------------------------------------------------------------------------------
/// \brief Get the SNResults for a point.
/// \param[in] a_p The (x, y, z) point whose parameters are desired.
/// \param[out] a_results: The vector of SNResult.
//------------------------------------------------------------------------------
void AnisotropicInterpolator::GetAllNormalPointParameters(const Pt3d& a_p, VecSNResult& a_results)
{
  a_results.clear();
  double lastParam = -1.0;
  for (size_t i = 0; i < m_lineParams.size(); ++i)
  {
    GetNormalPointParameter(i, a_p, lastParam, a_results);
  }
} // AnisotropicInterpolator::GetAllNormalPointParameters
//------------------------------------------------------------------------------
/// \brief Transforms a point from (x, y, z) to (s, n, z) coordinates.
/// s is for station (distance along the centerline) and n for normal
/// (perpendicular distance) to a segment to m_centerLinePts. The z coordinate
/// is left unchanged.
/// \param[in] a_p  The (x, y, z) point to transform.
/// \param[in] a_onlyClosest  True to return only one (the closest) (s, n, z)
///                           point.
/// \return A vector of (s, n, z) points corresponding to a_p.  Conceptually,
/// the centerline is straightened out to lie along the x axis, with this first
/// point at x = 0. The point will be projected via a normal through it onto
/// each centerline segment. Their s-value will be the accumulated distance
/// along the centerline to that intersection. The n-value will be the distance
/// along that normal. It will be positive if the point is to the left of the
/// segment and negative if to the right. A (s, n, z) point will be generated
/// for each segment where the normal is between the segment end points. If
/// a_onlyClosest is true, only the (s, n, z) point with the smallest
/// absolute value of n will be returned.
//------------------------------------------------------------------------------
VecPt3d AnisotropicInterpolator::TransformPoint(const Pt3d& a_p, bool a_onlyClosest)
{
  double value = a_p.z;

  VecSNResult results;
  GetAllNormalPointParameters(a_p, results);
  VecPt3d snPoints;
  for (auto& result : results)
  {
    // const Pt3d& closest = result.m_position;
    double len0 = m_accLengths[result.m_index];
    double len1 = m_accLengths[result.m_index + 1];
    double s = len0 + result.m_param * (len1 - len0);
    double n = (result.m_cross == 0.0) ? 0.0 // Collinear
                                       : result.m_cross < 0.0 ? -Distance(a_p, result.m_position)
                                                              : Distance(a_p, result.m_position);
    Pt3d snv(s, n, value);
    if (a_onlyClosest)
    {
      if (snPoints.size() == 0)
      {
        snPoints.push_back(snv);
      }
      else if (abs(snv.y) < abs(snPoints[0].y))
      {
        snPoints[0] = snv;
      }
    }
    else
    {
      snPoints.push_back(snv);
    }
  }
  return snPoints;
} // AnisotropicInterpolator::TransformPoint

} // namespace xms

#ifdef CXX_TEST
////////////////////////////////////////////////////////////////////////////////

#include <xmsinterp/interpolate/detail/AnisotropicInterpolator.t.h>
#include <xmscore/testing/TestTools.h>

// namespace xms {
using namespace xms;

////////////////////////////////////////////////////////////////////////////////
/// \class AnisotropicInterpolatorUnitTests
/// \brief Unit tests for the AnisotropicInterpolator class
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
/// \brief Tests simple interpolation problem.
//------------------------------------------------------------------------------
void AnisotropicInterpolatorUnitTests::testSimple()
{
  VecPt3d centerline = {{-1, 0, 0}, {0, 0, 0}, {1, 0, 0}};
  VecPt3d crossSections = {
    {-0.5, -1, 0}, {-0.5, 0, -1}, {-0.5, 1, 0},
    {0.5, -1, 0}, {0.5, 0, -1}, {0.5, 1, 0}
  };
  Pt3d target(0, 0, 0);
  
  AnisotropicInterpolator interpolator;
  bool pickClosest = true;
  interpolator.SetPoints(centerline, crossSections, pickClosest);
  Pt3d sn0 = interpolator.TransformPoint(crossSections[0], pickClosest)[0];
  Pt3d sn1 = interpolator.TransformPoint(crossSections[1], pickClosest)[0];
  Pt3d sn2 = interpolator.TransformPoint(crossSections[2], pickClosest)[0];
  Pt3d sn3 = interpolator.TransformPoint(crossSections[3], pickClosest)[0];
  Pt3d sn4 = interpolator.TransformPoint(crossSections[4], pickClosest)[0];
  Pt3d sn5 = interpolator.TransformPoint(crossSections[5], pickClosest)[0];

  VecPt3d snPoints = interpolator.GetInterpolationPoints();
  double xScale = 1.0;
  
  VecPt3d snPoints2 = {sn1, sn4};
  double result = interpolator.InterpolatePoint(target, snPoints2, xScale);
  double expect(-1.0);
  TS_ASSERT_EQUALS(expect, result);
  
  VecPt3d snPoints3 = {sn0, sn2, sn3, sn5};
  result = interpolator.InterpolatePoint(target, snPoints3, xScale);
  expect = 0.0;
  TS_ASSERT_EQUALS(expect, result); // -0.6250
  
  result = interpolator.InterpolatePoint(target, snPoints, xScale);
  expect = -0.714;
  TS_ASSERT_DELTA(expect, result, 0.01);
} // AnisotropicInterpolatorUnitTests::testSimple
//------------------------------------------------------------------------------
/// \brief Tests interpolation of crosssection going through a centerline point.
// +                         +
//  \                       /
//   \                     /
//    +-------------------+
//    |\                 /|
//    | \               / |
//    |  \             /  |
//    |   +           +   |
//    +                   +
//------------------------------------------------------------------------------
void AnisotropicInterpolatorUnitTests::testCrossSectionThroughPoint()
{
  VecPt3d centerline = {{-10, 0, 0}, {-10, 10, 0}, {10, 10, 0}, {10, 0, 0}};
  VecPt3d crossSections = {
    {-15, 15, 0}, {-10, 10, -1}, {-5, 5, 0},
    { 5, 5, 0}, {10, 10, -1}, {15, 15, 0}
  };
  Pt3d target(0, 5, 0);
  
  AnisotropicInterpolator interpolator;
  bool pickClosest = false;
  interpolator.SetPoints(centerline, crossSections, pickClosest);
  
  double cross = interpolator.CrossProduct(0, crossSections[2]);
  TS_ASSERT(cross < 0.0);
  
  VecPt3d sn2 = interpolator.TransformPoint(crossSections[2]);
  VecPt3d sn2Expected = {{5, -5, 0}, {15, -5, 0}, {35, -15, 0}};
  TS_ASSERT_DELTA_VECPT3D(sn2Expected, sn2, 0.001);
  VecPt3d sn3 = interpolator.TransformPoint(crossSections[3]);
  VecPt3d sn3Expected = {{5, -15, 0}, {25, -5, 0}, {35, -5, 0}};
  TS_ASSERT_DELTA_VECPT3D(sn3Expected, sn3, 0.001);

  VecPt3d snPoints = interpolator.GetInterpolationPoints();
  double xScale = 1.0;
  double result = interpolator.InterpolatePoint(target, snPoints, xScale);
  double expect = -0.2864;
  TS_ASSERT_DELTA(expect, result, 0.01);
  
  Pt3d target2(0, 15, 0);
  double result2 = interpolator.InterpolatePoint(target, snPoints, xScale);
  TS_ASSERT_DELTA(result, result2, 1.0e-6);
  
  Pt3d target3 = crossSections[1];
  target3.z = 10;
  double result3 = interpolator.InterpolatePoint(target3, snPoints, xScale);
  TS_ASSERT_DELTA(crossSections[1].z, result3, 1.0e-6);
} // AnisotropicInterpolatorUnitTests::testCrossSectionThroughPoint
//------------------------------------------------------------------------------
/// \brief Tests complex interpolation problem replicating python test.
//------------------------------------------------------------------------------
void AnisotropicInterpolatorUnitTests::testComplex()
{
  VecPt3d centerline = {
    {-2.590967667524272, 1.5122455309386216, 0},
    {-2.349372473346755, 1.8655961464048296, 0},
    {-2.059947998563715, 2.1809663553602436, 0},
    {-1.728586439740639, 2.4519357496363527, 0},
    {-1.3620337620910616, 2.6729878471336286, 0},
    {-0.9677523629465701, 2.8396223981387614, 0},
    {-0.5537691498712743, 2.948447002856054, 0},
    {-0.12851212629536304, 2.997246174973795, 0},
    {0.2993611884900206, 2.9850264452473185, 0},
    {0.7211400118956299, 2.912036586865483, 0},
    {1.128237634948324, 2.7797625508460633, 0},
    {1.5123662332746903, 2.5908972145669007, 0},
    {1.86570559329583, 2.3492855593019457, 0},
    {2.1810623186511595, 2.0598463928555515, 0},
    {2.4520162756756365, 1.7284722108908146, 0},
    {2.673051296546267, 1.361909235607249, 0},
    {2.8396674792012004, 0.9676200739789866, 0},
    {2.9484727977935217, 0.5536317916012801, 0},
    {2.997252158644656, 0.12837249510680593, 0},
    {2.9850124958340776, -0.29950024994048446, 0}
  };
  VecPt3d crossSections = {
    {2.0, 0.0, 1},
    {2.05, 0.0, 1},
    {2.1, 0.0, 1},
    {2.15, 0.0, 1},
    {2.2, 0.0, 1},
    {2.25, 0.0, 1},
    {2.3, 0.0, 1},
    {2.35, 0.0, 1},
    {2.4, 0.0, 1},
    {2.45, 0.0, 1},
    {2.5, 0.0, 1},
    {2.55, 0.0, 0.9000000000000004},
    {2.6, 0.0, 0.7999999999999998},
    {2.65, 0.0, 0.7000000000000002},
    {2.7, 0.0, 0.5999999999999996},
    {2.75, 0.0, 0.5},
    {2.8, 0.0, 0.40000000000000036},
    {2.85, 0.0, 0.2999999999999998},
    {2.9, 0.0, 0.20000000000000018},
    {2.95, 0.0, 0.09999999999999964},
    {3.0, 0.0, 0.0},
    {3.05, 0.0, 0.09999999999999964},
    {3.1, 0.0, 0.20000000000000018},
    {3.1500000000000004, 0.0, 0.3000000000000007},
    {3.2, 0.0, 0.40000000000000036},
    {3.25, 0.0, 0.5},
    {3.3, 0.0, 0.5999999999999996},
    {3.35, 0.0, 0.7000000000000002},
    {3.4000000000000004, 0.0, 0.8000000000000007},
    {3.45, 0.0, 0.9000000000000004},
    {3.5, 0.0, 1},
    {3.55, 0.0, 1},
    {3.6, 0.0, 1},
    {3.6500000000000004, 0.0, 1},
    {3.7, 0.0, 1},
    {3.75, 0.0, 1},
    {3.8, 0.0, 1},
    {3.85, 0.0, 1},
    {3.9000000000000004, 0.0, 1},
    {3.95, 0.0, 1},
    {4.0, 0.0, 1},
    {1.8270909152852017, 0.8134732861516003, 1},
    {1.8727681881673317, 0.8338101183053902, 1},
    {1.918445461049462, 0.8541469504591803, 1},
    {1.9641227339315919, 0.8744837826129703, 1},
    {2.0098000068137223, 0.8948206147667604, 1},
    {2.055477279695852, 0.9151574469205503, 1},
    {2.101154552577982, 0.9354942790743402, 1},
    {2.146831825460112, 0.9558311112281304, 1},
    {2.192509098342242, 0.9761679433819204, 1},
    {2.2381863712243724, 0.9965047755357105, 1},
    {2.283863644106502, 1.0168416076895004, 1},
    {2.329540916988632, 1.0371784398432904, 0.9000000000000004},
    {2.3752181898707625, 1.0575152719970804, 0.7999999999999998},
    {2.4208954627528922, 1.0778521041508704, 0.7000000000000002},
    {2.4665727356350224, 1.0981889363046604, 0.5999999999999996},
    {2.512250008517152, 1.1185257684584504, 0.5},
    {2.5579272813992824, 1.1388626006122404, 0.40000000000000036},
    {2.6036045542814126, 1.1591994327660304, 0.2999999999999998},
    {2.6492818271635423, 1.1795362649198204, 0.20000000000000018},
    {2.6949591000456725, 1.1998730970736105, 0.09999999999999964},
    {2.7406363729278027, 1.2202099292274005, 0.0},
    {2.7863136458099325, 1.2405467613811905, 0.09999999999999964},
    {2.8319909186920627, 1.2608835935349805, 0.20000000000000018},
    {2.877668191574193, 1.2812204256887707, 0.3000000000000007},
    {2.923345464456323, 1.3015572578425605, 0.40000000000000036},
    {2.969022737338453, 1.3218940899963505, 0.5},
    {3.0147000102205825, 1.3422309221501405, 0.5999999999999996},
    {3.060377283102713, 1.3625677543039305, 0.7000000000000002},
    {3.1060545559848434, 1.3829045864577207, 0.8000000000000007},
    {3.151731828866973, 1.4032414186115105, 0.9000000000000004},
    {3.197409101749103, 1.4235782507653005, 1},
    {3.243086374631233, 1.4439150829190905, 1},
    {3.2887636475133633, 1.4642519150728805, 1},
    {3.3344409203954934, 1.4845887472266708, 1},
    {3.380118193277623, 1.5049255793804606, 1},
    {3.4257954661597534, 1.5252624115342506, 1},
    {3.471472739041883, 1.5455992436880406, 1},
    {3.5171500119240133, 1.5659360758418306, 1},
    {3.5628272848061435, 1.5862729079956208, 1},
    {3.6085045576882737, 1.6066097401494106, 1},
    {3.6541818305704035, 1.6269465723032006, 1},
    {1.3382612127177165, 1.4862896509547883, 1},
    {1.3717177430356593, 1.523446892228658, 1},
    {1.4051742733536023, 1.5606041335025278, 1},
    {1.4386308036715452, 1.5977613747763972, 1},
    {1.4720873339894882, 1.6349186160502671, 1},
    {1.505543864307431, 1.6720758573241368, 1},
    {1.5390003946253739, 1.7092330985980064, 1},
    {1.572456924943317, 1.7463903398718763, 1},
    {1.6059134552612597, 1.783547581145746, 1},
    {1.6393699855792028, 1.8207048224196158, 1},
    {1.6728265158971456, 1.8578620636934853, 1},
    {1.7062830462150884, 1.895019304967355, 0.9000000000000004},
    {1.7397395765330315, 1.9321765462412248, 0.7999999999999998},
    {1.7731961068509743, 1.9693337875150945, 0.7000000000000002},
    {1.8066526371689173, 2.006491028788964, 0.5999999999999996},
    {1.8401091674868602, 2.043648270062834, 0.5},
    {1.873565697804803, 2.0808055113367034, 0.40000000000000036},
    {1.907022228122746, 2.1179627526105733, 0.2999999999999998},
    {1.9404787584406888, 2.1551199938844428, 0.20000000000000018},
    {1.973935288758632, 2.1922772351583126, 0.09999999999999964},
    {2.0073918190765747, 2.2294344764321825, 0.0},
    {2.0408483493945173, 2.266591717706052, 0.09999999999999964},
    {2.074304879712461, 2.303748958979922, 0.20000000000000018},
    {2.107761410030404, 2.3409062002537917, 0.3000000000000007},
    {2.1412179403483464, 2.378063441527661, 0.40000000000000036},
    {2.1746744706662895, 2.415220682801531, 0.5},
    {2.208131000984232, 2.4523779240754005, 0.5999999999999996},
    {2.241587531302175, 2.4895351653492703, 0.7000000000000002},
    {2.275044061620118, 2.52669240662314, 0.8000000000000007},
    {2.3085005919380612, 2.56384964789701, 0.9000000000000004},
    {2.341957122256004, 2.6010068891708795, 1},
    {2.3754136525739464, 2.638164130444749, 1},
    {2.40887018289189, 2.675321371718619, 1},
    {2.442326713209833, 2.7124786129924887, 1},
    {2.4757832435277756, 2.7496358542663586, 1},
    {2.509239773845718, 2.786793095540228, 1},
    {2.542696304163661, 2.8239503368140975, 1},
    {2.5761528344816043, 2.8611075780879673, 1},
    {2.6096093647995473, 2.898264819361837, 1},
    {2.6430658951174903, 2.935422060635707, 1},
    {2.676522425435433, 2.9725793019095765, 1},
    {0.6180339887498949, 1.902113032590307, 1},
    {0.6334848384686422, 1.9496658584050646, 1},
    {0.6489356881873897, 1.9972186842198225, 1},
    {0.664386537906137, 2.04477151003458, 1},
    {0.6798373876248844, 2.0923243358493377, 1},
    {0.6952882373436318, 2.1398771616640953, 1},
    {0.7107390870623791, 2.1874299874788528, 1},
    {0.7261899367811265, 2.2349828132936107, 1},
    {0.7416407864998739, 2.282535639108368, 1},
    {0.7570916362186213, 2.330088464923126, 1},
    {0.7725424859373686, 2.3776412907378837, 1},
    {0.787993335656116, 2.425194116552641, 0.9000000000000004},
    {0.8034441853748634, 2.472746942367399, 0.7999999999999998},
    {0.8188950350936107, 2.5202997681821566, 0.7000000000000002},
    {0.8343458848123582, 2.5678525939969146, 0.5999999999999996},
    {0.8497967345311055, 2.615405419811672, 0.5},
    {0.8652475842498528, 2.6629582456264296, 0.40000000000000036},
    {0.8806984339686003, 2.7105110714411875, 0.2999999999999998},
    {0.8961492836873476, 2.758063897255945, 0.20000000000000018},
    {0.911600133406095, 2.805616723070703, 0.09999999999999964},
    {0.9270509831248424, 2.8531695488854605, 0.0},
    {0.9425018328435897, 2.900722374700218, 0.09999999999999964},
    {0.9579526825623371, 2.948275200514976, 0.20000000000000018},
    {0.9734035322810846, 2.995828026329734, 0.3000000000000007},
    {0.9888543819998319, 3.0433808521444914, 0.40000000000000036},
    {1.0043052317185792, 3.090933677959249, 0.5},
    {1.0197560814373265, 3.1384865037740064, 0.5999999999999996},
    {1.035206931156074, 3.1860393295887643, 0.7000000000000002},
    {1.0506577808748214, 3.2335921554035223, 0.8000000000000007},
    {1.0661086305935688, 3.28114498121828, 0.9000000000000004},
    {1.081559480312316, 3.3286978070330373, 1},
    {1.0970103300310634, 3.376250632847795, 1},
    {1.112461179749811, 3.4238034586625528, 1},
    {1.1279120294685583, 3.4713562844773107, 1},
    {1.1433628791873056, 3.518909110292068, 1},
    {1.158813728906053, 3.5664619361068257, 1},
    {1.1742645786248003, 3.614014761921583, 1},
    {1.1897154283435478, 3.661567587736341, 1},
    {1.2051662780622951, 3.709120413551099, 1},
    {1.2206171277810425, 3.7566732393658566, 1},
    {1.2360679774997898, 3.804226065180614, 1},
    {-0.20905692653530666, 1.9890437907365468, 1},
    {-0.2142833496986893, 2.0387698855049603, 1},
    {-0.21950977286207202, 2.088495980273374, 1},
    {-0.22473619602545467, 2.1382220750417877, 1},
    {-0.22996261918883734, 2.1879481698102015, 1},
    {-0.23518904235222, 2.237674264578615, 1},
    {-0.24041546551560264, 2.2874003593470285, 1},
    {-0.24564188867898534, 2.3371264541154426, 1},
    {-0.25086831184236796, 2.386852548883856, 1},
    {-0.2560947350057507, 2.43657864365227, 1},
    {-0.2613211581691333, 2.4863047384206833, 1},
    {-0.266547581332516, 2.536030833189097, 0.9000000000000004},
    {-0.27177400449589867, 2.5857569279575108, 0.7999999999999998},
    {-0.27700042765928135, 2.6354830227259245, 0.7000000000000002},
    {-0.282226850822664, 2.685209117494338, 0.5999999999999996},
    {-0.28745327398604664, 2.734935212262752, 0.5},
    {-0.2926796971494293, 2.784661307031165, 0.40000000000000036},
    {-0.297906120312812, 2.8343874017995794, 0.2999999999999998},
    {-0.30313254347619467, 2.8841134965679927, 0.20000000000000018},
    {-0.30835896663957735, 2.933839591336407, 0.09999999999999964},
    {-0.31358538980296, 2.98356568610482, 0.0},
    {-0.31881181296634264, 3.033291780873234, 0.09999999999999964},
    {-0.3240382361297253, 3.0830178756416475, 0.20000000000000018},
    {-0.32926465929310805, 3.1327439704100617, 0.3000000000000007},
    {-0.3344910824564907, 3.182470065178475, 0.40000000000000036},
    {-0.33971750561987335, 3.2321961599468887, 0.5},
    {-0.34494392878325597, 3.281922254715302, 0.5999999999999996},
    {-0.35017035194663865, 3.331648349483716, 0.7000000000000002},
    {-0.3553967751100214, 3.38137444425213, 0.8000000000000007},
    {-0.360623198273404, 3.4311005390205436, 0.9000000000000004},
    {-0.3658496214367867, 3.480826633788957, 1},
    {-0.3710760446001693, 3.5305527285573706, 1},
    {-0.37630246776355203, 3.5802788233257843, 1},
    {-0.3815288909269347, 3.6300049180941985, 1},
    {-0.3867553140903173, 3.6797310128626117, 1},
    {-0.3919817372537, 3.7294571076310254, 1},
    {-0.3972081604170826, 3.7791832023994387, 1},
    {-0.40243458358046535, 3.828909297167853, 1},
    {-0.40766100674384803, 3.8786353919362666, 1},
    {-0.4128874299072307, 3.9283614867046803, 1},
    {-0.41811385307061333, 3.9780875814730936, 1},
    {-0.9999999999999996, 1.7320508075688774, 1},
    {-1.0249999999999995, 1.7753520777580991, 1},
    {-1.0499999999999996, 1.8186533479473213, 1},
    {-1.0749999999999995, 1.8619546181365432, 1},
    {-1.0999999999999996, 1.9052558883257653, 1},
    {-1.1249999999999996, 1.948557158514987, 1},
    {-1.1499999999999995, 1.991858428704209, 1},
    {-1.1749999999999996, 2.035159698893431, 1},
    {-1.1999999999999995, 2.078460969082653, 1},
    {-1.2249999999999996, 2.121762239271875, 1},
    {-1.2499999999999996, 2.165063509461097, 1},
    {-1.2749999999999992, 2.2083647796503185, 0.9000000000000004},
    {-1.2999999999999994, 2.2516660498395407, 0.7999999999999998},
    {-1.3249999999999993, 2.2949673200287624, 0.7000000000000002},
    {-1.3499999999999994, 2.3382685902179845, 0.5999999999999996},
    {-1.3749999999999993, 2.3815698604072066, 0.5},
    {-1.3999999999999992, 2.4248711305964283, 0.40000000000000036},
    {-1.4249999999999994, 2.4681724007856505, 0.2999999999999998},
    {-1.4499999999999993, 2.511473670974872, 0.20000000000000018},
    {-1.4749999999999994, 2.5547749411640943, 0.09999999999999964},
    {-1.4999999999999993, 2.598076211353316, 0.0},
    {-1.5249999999999992, 2.6413774815425377, 0.09999999999999964},
    {-1.5499999999999994, 2.68467875173176, 0.20000000000000018},
    {-1.5749999999999995, 2.7279800219209824, 0.3000000000000007},
    {-1.5999999999999994, 2.771281292110204, 0.40000000000000036},
    {-1.6249999999999993, 2.814582562299426, 0.5},
    {-1.6499999999999992, 2.8578838324886475, 0.5999999999999996},
    {-1.6749999999999994, 2.9011851026778697, 0.7000000000000002},
    {-1.6999999999999995, 2.944486372867092, 0.8000000000000007},
    {-1.7249999999999994, 2.9877876430563135, 0.9000000000000004},
    {-1.7499999999999991, 3.0310889132455356, 1},
    {-1.774999999999999, 3.0743901834347573, 1},
    {-1.7999999999999992, 3.1176914536239795, 1},
    {-1.8249999999999993, 3.1609927238132016, 1},
    {-1.8499999999999992, 3.2042939940024233, 1},
    {-1.8749999999999991, 3.247595264191645, 1},
    {-1.899999999999999, 3.290896534380867, 1},
    {-1.9249999999999992, 3.3341978045700893, 1},
    {-1.9499999999999993, 3.3774990747593114, 1},
    {-1.9749999999999992, 3.420800344948533, 1},
    {-1.9999999999999991, 3.464101615137755, 1},
    {-1.6180339887498947, 1.1755705045849465, 1},
    {-1.658484838468642, 1.20495976719957, 1},
    {-1.6989356881873894, 1.234349029814194, 1},
    {-1.7393865379061366, 1.2637382924288174, 1},
    {-1.7798373876248843, 1.2931275550434413, 1},
    {-1.8202882373436315, 1.3225168176580648, 1},
    {-1.8607390870623788, 1.3519060802726883, 1},
    {-1.9011899367811262, 1.3812953428873123, 1},
    {-1.9416407864998735, 1.4106846055019358, 1},
    {-1.9820916362186212, 1.4400738681165595, 1},
    {-2.022542485937368, 1.4694631307311832, 1},
    {-2.0629933356561154, 1.4988523933458067, 0.9000000000000004},
    {-2.103444185374863, 1.5282416559604306, 0.7999999999999998},
    {-2.1438950350936103, 1.557630918575054, 0.7000000000000002},
    {-2.184345884812358, 1.5870201811896778, 0.5999999999999996},
    {-2.2247967345311053, 1.6164094438043015, 0.5},
    {-2.2652475842498525, 1.645798706418925, 0.40000000000000036},
    {-2.3056984339686, 1.6751879690335487, 0.2999999999999998},
    {-2.3461492836873474, 1.7045772316481724, 0.20000000000000018},
    {-2.3866001334060947, 1.7339664942627961, 0.09999999999999964},
    {-2.427050983124842, 1.7633557568774196, 0.0},
    {-2.467501832843589, 1.7927450194920433, 0.09999999999999964},
    {-2.507952682562337, 1.822134282106667, 0.20000000000000018},
    {-2.5484035322810845, 1.851523544721291, 0.3000000000000007},
    {-2.5888543819998318, 1.8809128073359145, 0.40000000000000036},
    {-2.629305231718579, 1.910302069950538, 0.5},
    {-2.6697560814373262, 1.9396913325651617, 0.5999999999999996},
    {-2.7102069311560735, 1.9690805951797854, 0.7000000000000002},
    {-2.750657780874821, 1.9984698577944093, 0.8000000000000007},
    {-2.7911086305935684, 2.027859120409033, 0.9000000000000004},
    {-2.8315594803123156, 2.0572483830236563, 1},
    {-2.872010330031063, 2.08663764563828, 1},
    {-2.9124611797498106, 2.1160269082529037, 1},
    {-2.9529120294685582, 2.1454161708675277, 1},
    {-2.9933628791873055, 2.174805433482151, 1},
    {-3.0338137289060527, 2.2041946960967747, 1},
    {-3.0742645786248, 2.233583958711398, 1},
    {-3.114715428343547, 2.262973221326022, 1},
    {-3.155166278062295, 2.292362483940646, 1},
    {-3.195617127781042, 2.3217517465552695, 1},
    {-3.2360679774997894, 2.351141009169893, 1}
  };
  Pt3d target(1.6, 2.6, 0);
  
  AnisotropicInterpolator interpolator;
  bool pickClosest = true;
  interpolator.SetPoints(centerline, crossSections, pickClosest);
  VecPt3d snPoints = interpolator.GetInterpolationPoints();
  VecPt3d expectedSnPoints = {
    {7.861695483191204, -0.9931736833190422, 1.0},
    {7.8602657756205705, -0.9431941281363231, 1.0},
    {7.858836068049938, -0.8932145729536034, 1.0},
    {7.857406360479321, -0.8432350177708842, 1.0},
    {7.855976652908689, -0.7932554625881644, 1.0},
    {7.854546945338057, -0.7432759074054452, 1.0},
    {7.85311723776744, -0.693296352222726, 1.0},
    {7.851687530196807, -0.6433167970400063, 1.0},
    {7.850257822626175, -0.5933372418572872, 1.0},
    {7.848828115055558, -0.5433576866745674, 1.0},
    {7.847398407484926, -0.4933781314918483, 1.0},
    {7.845968699914293, -0.4433985763091291, 0.9000000000000004},
    {7.844538992343677, -0.39341902112640936, 0.7999999999999998},
    {7.8431092847730435, -0.3434394659436902, 0.7000000000000002},
    {7.841679577202411, -0.29345991076097055, 0.5999999999999996},
    {7.840249869631794, -0.24348035557825168, 0.5},
    {7.838820162061162, -0.1935008003955325, 0.40000000000000036},
    {7.83739045449053, -0.14352124521281284, 0.2999999999999998},
    {7.835960746919913, -0.0935416900300936, 0.20000000000000018},
    {7.834531039349265, -0.04356213484737399, 0.09999999999999964},
    {7.833101331778632, 0.006417420335345209, 0.0},
    {7.831671624208016, 0.05639697551806445, 0.09999999999999964},
    {7.830241916637383, 0.1063765307007841, 0.20000000000000018},
    {7.828812209066751, 0.15635608588350375, 0.3000000000000007},
    {7.827382501496134, 0.20633564106622299, 0.40000000000000036},
    {7.825952793925501, 0.25631519624894217, 0.5},
    {7.824523086354869, 0.3062947514316614, 0.5999999999999996},
    {7.823093378784252, 0.3562743066143811, 0.7000000000000002},
    {7.82166367121362, 0.40625386179710077, 0.8000000000000007},
    {7.820233963643003, 0.45623341697982, 0.9000000000000004},
    {7.8188042560723705, 0.5062129721625391, 1.0},
    {7.817374548501738, 0.5561925273452584, 1.0},
    {7.815944840931121, 0.606172082527978, 1.0},
    {7.814515133360489, 0.6561516377106977, 1.0},
    {7.813085425789856, 0.706131192893417, 1.0},
    {7.81165571821924, 0.7561107480761358, 1.0},
    {7.8102260106486066, 0.8060903032588549, 1.0},
    {7.808796303077974, 0.8560698584415747, 1.0},
    {7.807366595507357, 0.9060494136242943, 1.0},
    {7.805936887936725, 0.9560289688070135, 1.0},
    {7.804507180366093, 1.0060085239897327, 1.0},
    {6.596612046804048, -0.9927193678084792, 1.0},
    {6.595658836240024, -0.9427284547379958, 1.0},
    {6.594705625676, -0.8927375416675122, 1.0},
    {6.593752415111976, -0.8427466285970288, 1.0},
    {6.592799204547952, -0.7927557155265451, 1.0},
    {6.591845993983928, -0.742764802456062, 1.0},
    {6.590892783419904, -0.6927738893855788, 1.0},
    {6.589939572855879, -0.6427829763150952, 1.0},
    {6.588986362291855, -0.5927920632446115, 1.0},
    {6.5880331517278305, -0.5428011501741281, 1.0},
    {6.5870799411638075, -0.49281023710364497, 1.0},
    {6.586126730599783, -0.4428193240331617, 0.9000000000000004},
    {6.5851735200357595, -0.39282841096267773, 0.7999999999999998},
    {6.5842203094717355, -0.3428374978921945, 0.7000000000000002},
    {6.58326709890771, -0.2928465848217108, 0.5999999999999996},
    {6.582313888343686, -0.24285567175122758, 0.5},
    {6.581360677779662, -0.1928647586807443, 0.40000000000000036},
    {6.580407467215638, -0.14287384561026065, 0.2999999999999998},
    {6.579454256651614, -0.09288293253977742, 0.20000000000000018},
    {6.57850104608759, -0.04289201946929378, 0.09999999999999964},
    {6.577547835523566, 0.0070988936011898525, 0.0},
    {6.576594624959541, 0.05708980667167308, 0.09999999999999964},
    {6.575641414395517, 0.10708071974215637, 0.20000000000000018},
    {6.574688203831493, 0.15707163281264008, 0.3000000000000007},
    {6.573734993267469, 0.20706254588312364, 0.40000000000000036},
    {6.572781782703445, 0.2570534589536069, 0.5},
    {6.57182857213942, 0.3070443720240902, 0.5999999999999996},
    {6.570875361575397, 0.35703528509457416, 0.7000000000000002},
    {6.569922151011373, 0.40702619816505786, 0.8000000000000007},
    {6.5689689404473475, 0.45701711123554073, 0.9000000000000004},
    {6.5680157298833235, 0.5070080243060239, 1.0},
    {6.5670625193192995, 0.5569989373765075, 1.0},
    {6.5661093087552755, 0.6069898504469912, 1.0},
    {6.565156098191252, 0.6569807635174749, 1.0},
    {6.564202887627228, 0.7069716765879578, 1.0},
    {6.563249677063203, 0.7569625896584413, 1.0},
    {6.562296466499179, 0.8069535027289246, 1.0},
    {6.561343255935155, 0.8569444157994082, 1.0},
    {6.560390045371131, 0.9069353288698919, 1.0},
    {6.559436834807107, 0.9569262419403755, 1.0},
    {6.558483624243083, 1.0069171550108587, 1.0},
    {5.331532075198482, -0.9924467619873838, 1.0},
    {5.331055448260601, -0.942449033771373, 1.0},
    {5.33057882132272, -0.8924513055553617, 1.0},
    {5.330102194384839, -0.842453577339351, 1.0},
    {5.329625567446957, -0.7924558491233403, 1.0},
    {5.329148940509077, -0.7424581209073295, 1.0},
    {5.328672313571195, -0.6924603926913188, 1.0},
    {5.328195686633315, -0.6424626644753076, 1.0},
    {5.327719059695434, -0.5924649362592969, 1.0},
    {5.3272424327575525, -0.5424672080432857, 1.0},
    {5.326765805819671, -0.4924694798272755, 1.0},
    {5.326289178881791, -0.4424717516112648, 0.9000000000000004},
    {5.325812551943909, -0.39247402339525334, 0.7999999999999998},
    {5.325335925006029, -0.34247629517924255, 0.7000000000000002},
    {5.324859298068148, -0.292478566963232, 0.5999999999999996},
    {5.324382671130266, -0.24248083874722107, 0.5},
    {5.323906044192386, -0.19248311053121042, 0.40000000000000036},
    {5.323429417254505, -0.14248538231519933, 0.2999999999999998},
    {5.322952790316624, -0.09248765409918873, 0.20000000000000018},
    {5.322476163378743, -0.04248992588317764, 0.09999999999999964},
    {5.321999536440861, 0.0075078023328332515, 0.0},
    {5.32152290950298, 0.05750553054884372, 0.09999999999999964},
    {5.321046282565099, 0.10750325876485511, 0.20000000000000018},
    {5.320569655627218, 0.1575009869808662, 0.3000000000000007},
    {5.320093028689337, 0.20749871519687632, 0.40000000000000036},
    {5.319616401751456, 0.2574964434128874, 0.5},
    {5.319139774813576, 0.30749417162889786, 0.5999999999999996},
    {5.318663147875695, 0.357491899844909, 0.7000000000000002},
    {5.318186520937814, 0.40748962806092004, 0.8000000000000007},
    {5.3177098939999325, 0.45748735627693116, 0.9000000000000004},
    {5.3172332670620515, 0.5074850844929413, 1.0},
    {5.3167566401241695, 0.5574828127089518, 1.0},
    {5.316280013186289, 0.6074805409249633, 1.0},
    {5.315803386248408, 0.6574782691409743, 1.0},
    {5.315326759310527, 0.7074759973569847, 1.0},
    {5.314850132372646, 0.7574737255729953, 1.0},
    {5.314373505434766, 0.807471453789006, 1.0},
    {5.313896878496885, 0.8574691820050171, 1.0},
    {5.313420251559004, 0.9074669102210281, 1.0},
    {5.312943624621123, 0.9574646384370391, 1.0},
    {5.3124669976832415, 1.0074623666530496, 1.0},
    {4.066453836062427, -0.9923558906278168, 1.0},
    {4.066453836062427, -0.942355890627817, 1.0},
    {4.066453836062427, -0.8923558906278167, 1.0},
    {4.066453836062427, -0.8423558906278171, 1.0},
    {4.066453836062427, -0.7923558906278169, 1.0},
    {4.066453836062427, -0.742355890627817, 1.0},
    {4.066453836062427, -0.6923558906278172, 1.0},
    {4.066453836062427, -0.642355890627817, 1.0},
    {4.066453836062427, -0.5923558906278171, 1.0},
    {4.066453836062427, -0.5423558906278168, 1.0},
    {4.066453836062427, -0.492355890627817, 1.0},
    {4.066453836062427, -0.4423558906278172, 0.9000000000000004},
    {4.066453836062427, -0.3923558906278169, 0.7999999999999998},
    {4.066453836062427, -0.34235589062781707, 0.7000000000000002},
    {4.066453836062427, -0.2923558906278168, 0.5999999999999996},
    {4.066453836062427, -0.24235589062781696, 0.5},
    {4.066453836062427, -0.19235589062781713, 0.40000000000000036},
    {4.066453836062427, -0.14235589062781687, 0.2999999999999998},
    {4.066453836062427, -0.09235589062781704, 0.20000000000000018},
    {4.066453836062427, -0.042355890627816764, 0.09999999999999964},
    {4.066453836062427, 0.007644109372183057, 0.0},
    {4.066453836062427, 0.05764410937218288, 0.09999999999999964},
    {4.066453836062427, 0.10764410937218315, 0.20000000000000018},
    {4.066453836062427, 0.15764410937218346, 0.3000000000000007},
    {4.066453836062427, 0.20764410937218328, 0.40000000000000036},
    {4.066453836062427, 0.2576441093721831, 0.5},
    {4.066453836062427, 0.30764410937218295, 0.5999999999999996},
    {4.066453836062427, 0.3576441093721832, 0.7000000000000002},
    {4.066453836062427, 0.4076441093721835, 0.8000000000000007},
    {4.066453836062427, 0.4576441093721833, 0.9000000000000004},
    {4.066453836062427, 0.5076441093721831, 1.0},
    {4.066453836062427, 0.5576441093721829, 1.0},
    {4.066453836062427, 0.6076441093721833, 1.0},
    {4.066453836062427, 0.6576441093721835, 1.0},
    {4.066453836062427, 0.7076441093721834, 1.0},
    {4.066453836062427, 0.7576441093721832, 1.0},
    {4.066453836062427, 0.807644109372183, 1.0},
    {4.066453836062427, 0.8576441093721833, 1.0},
    {4.0664538360624265, 0.9076441093721832, 1.0},
    {4.0664538360624265, 0.9576441093721829, 1.0},
    {4.0664538360624265, 1.0076441093721826, 1.0},
    {2.801375596926374, -0.9924467619873834, 1.0},
    {2.801852223864255, -0.942449033771373, 1.0},
    {2.802328850802136, -0.8924513055553619, 1.0},
    {2.8028054777400166, -0.8424535773393513, 1.0},
    {2.8032821046778977, -0.7924558491233402, 1.0},
    {2.803758731615779, -0.7424581209073292, 1.0},
    {2.80423535855366, -0.692460392691319, 1.0},
    {2.804711985491541, -0.6424626644753074, 1.0},
    {2.805188612429422, -0.5924649362592973, 1.0},
    {2.8056652393673027, -0.5424672080432857, 1.0},
    {2.806141866305184, -0.4924694798272752, 1.0},
    {2.806618493243065, -0.44247175161126456, 0.9000000000000004},
    {2.807095120180946, -0.3924740233952535, 0.7999999999999998},
    {2.807571747118827, -0.34247629517924283, 0.7000000000000002},
    {2.8080483740567077, -0.29247856696323177, 0.5999999999999996},
    {2.808525000994589, -0.2424808387472207, 0.5},
    {2.80900162793247, -0.19248311053121056, 0.40000000000000036},
    {2.809478254870351, -0.14248538231519903, 0.2999999999999998},
    {2.8099548818082316, -0.09248765409918885, 0.20000000000000018},
    {2.8104315087461127, -0.04248992588317734, 0.09999999999999964},
    {2.810908135683994, 0.007507802332833277, 0.0},
    {2.811384762621875, 0.0575055305488439, 0.09999999999999964},
    {2.811861389559756, 0.10750325876485498, 0.20000000000000018},
    {2.812338016497637, 0.15750098698086604, 0.3000000000000007},
    {2.8128146434355177, 0.20749871519687668, 0.40000000000000036},
    {2.8132912703733988, 0.25749644341288774, 0.5},
    {2.81376789731128, 0.3074941716288979, 0.5999999999999996},
    {2.814244524249161, 0.3574918998449094, 0.7000000000000002},
    {2.814721151187042, 0.40748962806092004, 0.8000000000000007},
    {2.815197778124923, 0.4574873562769311, 0.9000000000000004},
    {2.8156744050628038, 0.5074850844929417, 1.0},
    {2.816151032000685, 0.5574828127089523, 1.0},
    {2.816627658938566, 0.6074805409249634, 1.0},
    {2.8171042858764466, 0.6574782691409745, 1.0},
    {2.8175809128143277, 0.7074759973569851, 1.0},
    {2.8180575397522087, 0.7574737255729962, 1.0},
    {2.81853416669009, 0.8074714537890063, 1.0},
    {2.819010793627971, 0.8574691820050179, 1.0},
    {2.819487420565852, 0.9074669102210285, 1.0},
    {2.8199640475037326, 0.9574646384370396, 1.0},
    {2.8204406744416137, 1.0074623666530502, 1.0},
    {1.5362956253208062, -0.9927193678084795, 1.0},
    {1.5372488358848306, -0.9427284547379963, 1.0},
    {1.5382020464488548, -0.8927375416675125, 1.0},
    {1.5391552570128788, -0.8427466285970292, 1.0},
    {1.540108467576903, -0.7927557155265457, 1.0},
    {1.5410616781409272, -0.7427648024560625, 1.0},
    {1.5420148887049514, -0.6927738893855792, 1.0},
    {1.5429680992689756, -0.6427829763150953, 1.0},
    {1.5439213098329998, -0.5927920632446118, 1.0},
    {1.5448745203970238, -0.5428011501741281, 1.0},
    {1.545827730961048, -0.4928102371036451, 1.0},
    {1.5467809415250722, -0.44281932403316204, 0.9000000000000004},
    {1.5477341520890961, -0.39282841096267807, 0.7999999999999998},
    {1.5486873626531203, -0.342837497892195, 0.7000000000000002},
    {1.5496405732171445, -0.2928465848217113, 0.5999999999999996},
    {1.550593783781169, -0.2428556717512277, 0.5},
    {1.551546994345193, -0.1928647586807446, 0.40000000000000036},
    {1.5525002049092171, -0.1428738456102609, 0.2999999999999998},
    {1.5534534154732413, -0.09288293253977782, 0.20000000000000018},
    {1.5544066260372653, -0.042892019469293864, 0.09999999999999964},
    {1.5553598366012897, 0.0070988936011893355, 0.0},
    {1.5563130471653137, 0.05708980667167242, 0.09999999999999964},
    {1.5572662577293377, 0.10708071974215638, 0.20000000000000018},
    {1.558219468293362, 0.15707163281264044, 0.3000000000000007},
    {1.559172678857386, 0.20706254588312353, 0.40000000000000036},
    {1.56012588942141, 0.2570534589536066, 0.5},
    {1.5610790999854345, 0.3070443720240898, 0.5999999999999996},
    {1.5620323105494585, 0.3570352850945734, 0.7000000000000002},
    {1.5629855211134827, 0.4070261981650571, 0.8000000000000007},
    {1.5639387316775069, 0.45701711123554056, 0.9000000000000004},
    {1.5648919422415313, 0.5070080243060238, 1.0},
    {1.5658451528055553, 0.5569989373765072, 1.0},
    {1.5667983633695797, 0.6069898504469909, 1.0},
    {1.5677515739336036, 0.6569807635174745, 1.0},
    {1.5687047844976276, 0.7069716765879576, 1.0},
    {1.569657995061652, 0.7569625896584408, 1.0},
    {1.570611205625676, 0.8069535027289246, 1.0},
    {1.5715644161897, 0.8569444157994082, 1.0},
    {1.5725176267537244, 0.9069353288698918, 1.0},
    {1.5734708373177484, 0.956926241940375, 1.0},
    {1.5744240478817724, 1.006917155010858, 1.0},
    {0.27121218893364296, -0.9931736833190418, 1.0},
    {0.27264189650427045, -0.9431941281363226, 1.0},
    {0.274071604074898, -0.8932145729536032, 1.0},
    {0.27550131164552544, -0.8432350177708837, 1.0},
    {0.27693101921615293, -0.793255462588164, 1.0},
    {0.27836072678677964, -0.7432759074054449, 1.0},
    {0.27979043435740714, -0.6932963522227258, 1.0},
    {0.28122014192803463, -0.6433167970400061, 1.0},
    {0.2826498494986621, -0.593337241857287, 1.0},
    {0.28407955706928967, -0.5433576866745674, 1.0},
    {0.2855092646399171, -0.4933781314918481, 1.0},
    {0.2869389722105446, -0.44339857630912904, 0.9000000000000004},
    {0.2883686797811721, -0.39341902112640936, 0.7999999999999998},
    {0.28979838735179964, -0.34343946594369, 0.7000000000000002},
    {0.2912280949224263, -0.29345991076097033, 0.5999999999999996},
    {0.29265780249305456, -0.24348035557825098, 0.5},
    {0.29408751006368133, -0.19350080039553202, 0.40000000000000036},
    {0.29551721763430877, -0.14352124521281223, 0.2999999999999998},
    {0.29694692520493626, -0.09354169003009301, 0.20000000000000018},
    {0.29837663277556375, -0.04356213484737381, 0.09999999999999964},
    {0.2998063403461913, 0.006417420335345516, 0.0},
    {0.30123604791681796, 0.05639697551806486, 0.09999999999999964},
    {0.3026657554874447, 0.10637653070078455, 0.20000000000000018},
    {0.30409546305807217, 0.15635608588350425, 0.3000000000000007},
    {0.30552517062869966, 0.20633564106622335, 0.40000000000000036},
    {0.3069548781993272, 0.25631519624894267, 0.5},
    {0.30838458576995464, 0.30629475143166185, 0.5999999999999996},
    {0.30981429334058214, 0.3562743066143811, 0.7000000000000002},
    {0.31124400091120963, 0.40625386179710105, 0.8000000000000007},
    {0.3126737084818372, 0.45623341697982006, 0.9000000000000004},
    {0.31410341605246467, 0.5062129721625391, 1.0},
    {0.3155331236230913, 0.5561925273452584, 1.0},
    {0.3169628311937189, 0.6061720825279783, 1.0},
    {0.3183925387643463, 0.656151637710698, 1.0},
    {0.3198222463349738, 0.7061311928934171, 1.0},
    {0.3212519539056013, 0.7561107480761364, 1.0},
    {0.322681661476228, 0.8060903032588556, 1.0},
    {0.32411136904685633, 0.8560698584415748, 1.0},
    {0.32554107661748377, 0.9060494136242948, 1.0},
    {0.32697078418811054, 0.9560289688070139, 1.0},
    {0.32840049175873803, 1.006008523989733, 1.0}
  };
  TS_ASSERT_DELTA_VECPT3D(expectedSnPoints, snPoints, 1.0e-5);
  double xScale = 1.0/20.0;
  int idwPower = 3;
  double result = interpolator.InterpolatePoint(target, snPoints, xScale, idwPower);
  double expect = 0.12388033238213704;
  TS_ASSERT_DELTA(expect, result, 1.0e-5);
  
  xScale = 1.0;
  result = interpolator.CalculateIDW(xScale, crossSections, target, idwPower);
  expect = 0.5933389848063627;
  TS_ASSERT_DELTA(expect, result, 1.0e-5);
} // AnisotropicInterpolatorUnitTests::testComplex

//} // namespace xms
#endif // CXX_TEST

