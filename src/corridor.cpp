#include "corridor/corridor.h"

#include "corridor/cubic_spline/cubic_spline.h"
#include "corridor/cubic_spline/cubic_spline_coefficients.h"

namespace corridor {

namespace cs = corridor::cubic_spline;

// /////////////////////////////////////////////////////////////////////////////
// Corridor
// /////////////////////////////////////////////////////////////////////////////

Corridor::Corridor(const IdType id, const CartesianPoints2D& reference_line_pts,
                   const RealType distance_left_boundary,
                   const RealType distance_right_boundary) {
  referenceLine_ = cs::CubicSpline(id, reference_line_pts);
  const auto num_pts = referenceLine_.GetSize();
  leftBound_ = FrenetPolyline(num_pts);
  rightBound_ = FrenetPolyline(num_pts);
  for (int i = 0; i < num_pts; i++) {
    const auto arc_length = referenceLine_.GetArclengthAtIndex(i);
    leftBound_.SetPoint(i, {arc_length, distance_left_boundary});
    rightBound_.SetPoint(i, {arc_length, -distance_right_boundary});
  }
}

Corridor::Corridor(const IdType id, const CartesianPoints2D& reference_line_pts,
                   const CartesianVector2D& first_tangent,
                   const CartesianVector2D& last_tangent,
                   const RealType distance_left_boundary,
                   const RealType distance_right_boundary) {
  referenceLine_ =
      cs::CubicSpline(id, reference_line_pts, first_tangent, last_tangent);
  const auto num_pts = referenceLine_.GetSize();
  leftBound_ = FrenetPolyline(num_pts);
  rightBound_ = FrenetPolyline(num_pts);
  for (int i = 0; i < num_pts; i++) {
    const auto arc_length = referenceLine_.GetArclengthAtIndex(i);
    leftBound_.SetPoint(i, {arc_length, distance_left_boundary});
    rightBound_.SetPoint(i, {arc_length, -distance_right_boundary});
  }
}  // namespace corridor

Corridor::Corridor(const IdType id, const CartesianPoints2D& reference_line_pts,
                   const CartesianPoints2D& left_boundary_pts,
                   const CartesianPoints2D& right_boundary_pts) {
  referenceLine_ = cs::CubicSpline(id, reference_line_pts);
  leftBound_ = referenceLine_.toFrenetPolyline(left_boundary_pts);
  rightBound_ = referenceLine_.toFrenetPolyline(right_boundary_pts);
}

Corridor::Corridor(const IdType id, const CartesianPoints2D& reference_line_pts,
                   const CartesianVector2D& first_tangent,
                   const CartesianVector2D& last_tangent,
                   const CartesianPoints2D& left_boundary_pts,
                   const CartesianPoints2D& right_boundary_pts) {
  referenceLine_ =
      cs::CubicSpline(id, reference_line_pts, first_tangent, last_tangent);
  leftBound_ = referenceLine_.toFrenetPolyline(left_boundary_pts);
  rightBound_ = referenceLine_.toFrenetPolyline(right_boundary_pts);
}

BoundaryDistances Corridor::signedDistancesAt(
    const RealType arc_length) const noexcept {
  return std::make_pair(leftBound_.deviationAt(arc_length),
                        rightBound_.deviationAt(arc_length));
}

RealType Corridor::widthAt(const RealType arc_length) const noexcept {
  return leftBound_.deviationAt(arc_length) +
         std::abs(rightBound_.deviationAt(arc_length));
}

RealType Corridor::centerOffset(const RealType arc_length) const noexcept {
  const BoundaryDistances distances = signedDistancesAt(arc_length);
  return (distances.first + distances.second) * 0.5;
}

RealType Corridor::lengthReferenceLine() const noexcept {
  return referenceLine_.GetTotalLength();
}

FrenetFrame2D Corridor::FrenetFrame(
    const CartesianPoint2D& position) const noexcept {
  return referenceLine_.getFrenetPositionWithFrame(position).frame;
}

FrenetPositionWithFrame Corridor::getFrenetPositionWithFrame(
    const CartesianPoint2D& position,
    const RealType arc_length_hint) const noexcept {
  return referenceLine_.getFrenetPositionWithFrame(position, arc_length_hint);
}

std::ostream& operator<<(std::ostream& os, const Corridor& corridor) {
  using namespace std;
  os << "Corridor " << corridor.id() << "\n";
  os << corridor.referenceLine_ << "\n";
  os << corridor.leftBound_ << "\n";
  os << corridor.rightBound_ << "\n";
  return os;
}

void Corridor::fillCartesianPolylines(
    const RealType delta_l, CartesianPoints2D* reference_line,
    CartesianPoints2D* left_boundary,
    CartesianPoints2D* right_boundary) const noexcept {
  reference_line->clear();
  left_boundary->clear();
  right_boundary->clear();
  RealType query_l = 0.0;
  RealType max_length = referenceLine_.GetTotalLength();
  while (query_l <= max_length) {
    const CartesianPoint2D position = referenceLine_.GetPositionAt(query_l);
    const CartesianVector2D normal = referenceLine_.GetNormalVectorAt(query_l);
    const RealType d_left = leftBound_.deviationAt(query_l);
    const RealType d_right = rightBound_.deviationAt(query_l);

    reference_line->emplace_back(position);
    left_boundary->emplace_back(position + d_left * normal);
    right_boundary->emplace_back(position + d_right * normal);

    query_l += delta_l;
  }

  if (query_l > max_length) {
    // Add last point
    const CartesianPoint2D position = referenceLine_.GetPositionAt(max_length);
    const CartesianVector2D normal =
        referenceLine_.GetNormalVectorAt(max_length);
    const RealType d_left = leftBound_.deviationAt(max_length);
    const RealType d_right = rightBound_.deviationAt(max_length);

    reference_line->emplace_back(position);
    left_boundary->emplace_back(position + d_left * normal);
    right_boundary->emplace_back(position + d_right * normal);
  }
}

// /////////////////////////////////////////////////////////////////////////////
// CorridorSequence
// /////////////////////////////////////////////////////////////////////////////

BoundaryDistances CorridorSequence::signedDistancesAt(
    const RealType arc_length) const noexcept {
  auto const_corridor_map_iter = this->get(arc_length);
  const auto delta_arc_length = arc_length - const_corridor_map_iter->first;
  return const_corridor_map_iter->second->signedDistancesAt(delta_arc_length);
}

RealType CorridorSequence::widthAt(const RealType arc_length) const noexcept {
  auto const_corridor_map_iter = this->get(arc_length);
  const auto delta_arc_length = arc_length - const_corridor_map_iter->first;
  return const_corridor_map_iter->second->widthAt(delta_arc_length);
}

RealType CorridorSequence::centerOffsetAt(
    const RealType arc_length) const noexcept {
  auto const_corridor_map_iter = this->get(arc_length);
  const auto delta_arc_length = arc_length - const_corridor_map_iter->first;
  return const_corridor_map_iter->second->centerOffset(delta_arc_length);
}

RealType CorridorSequence::curvatureAt(
    const RealType arc_length) const noexcept {
  auto const_corridor_map_iter = this->get(arc_length);
  const auto delta_arc_length = arc_length - const_corridor_map_iter->first;
  return const_corridor_map_iter->second->curvatureAt(delta_arc_length);
}

RealType CorridorSequence::totalLength() const noexcept {
  return corridor_map_.rbegin()->first /*offset to the last corridor*/ +
         corridor_map_.rbegin()->second->lengthReferenceLine();
}

FrenetPositionWithFrame CorridorSequence::getFrenetPositionWithFrame(
    const CartesianPoint2D& position,
    const RealType start_arc_length) const noexcept {
  CorridorSequenceMap::const_iterator iter = this->get(start_arc_length);
  return this->getFrenetPositionWithFrame(position, iter);
};

FrenetPositionWithFrame CorridorSequence::getFrenetPositionWithFrame(
    const CartesianPoint2D& position,
    CorridorSequenceMap::const_iterator iter) const {
  // Create frenet frame with converted position
  FrenetPositionWithFrame position_with_frame =
      iter->second->getFrenetPositionWithFrame(position);

  // If position is before current corridor, evaluate previous corridor in
  // the sequence
  if (position_with_frame.position.l() < 0.0 && iter != corridor_map_.begin()) {
    iter = std::prev(iter);
    position_with_frame = this->getFrenetPositionWithFrame(position, iter);
    return position_with_frame;
  }

  // If position is behind current corridor, evaluate next corridor in the
  // sequence
  if (iter->second->lengthReferenceLine() < position_with_frame.position.l() &&
      std::next(iter) != corridor_map_.end()) {
    iter = std::next(iter);
    position_with_frame = this->getFrenetPositionWithFrame(position, iter);
    return position_with_frame;
  }

  return position_with_frame;
}

std::ostream& operator<<(std::ostream& os, const CorridorPath& path) {
  using namespace std;
  os << "Corridor-Path:";
  for (const auto& c : path) {
    os << " -> " << c->id();
  }
  os << "\n";
  return os;
}

std::ostream& operator<<(std::ostream& os, const CorridorPaths& paths) {
  using namespace std;
  os << "--- Corridor-Paths ---\n";
  for (const auto& path : paths) {
    os << path << "\n";
  }
  return os;
}

}  // namespace corridor