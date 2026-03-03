#ifndef LARCOREALG_GEOMETRY_DETAILS_ELEMENT_ITERATORS_H
#define LARCOREALG_GEOMETRY_DETAILS_ELEMENT_ITERATORS_H

// LArSoft libraries
#include "larcorealg/Geometry/details/GeometryIterationPolicy.h"
#include "larcorealg/Geometry/details/ReadoutIterationPolicy.h"
#include "larcorealg/Geometry/details/id_iterators.h"
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

// Framework libraries
//#include "cetlib_except/exception.h"

// C/C++ standard libraries
#include <iterator> // std::forward_iterator_tag
#include <string>
#include <utility>

namespace geo::details {

  /**
   * @brief Forward iterator browsing all geometry elements in the detector
   * @tparam GEOITER type of geometry ID iterator
   *
   * This iterator works as the corresponding ID iterator in the template argument. The
   * difference is the dereferenciation operator: this one obtains the geometry element
   * directly, or throws on failure.  The boolean conversion operator checks that it can
   * obtain a pointer to the geometry element.
   *
   * In particular, get() and ID() methods still return the pointer to the geometry
   * element and its ID, respectively.
   *
   * It can also be initialized and compare with the corresponding ID iterator.
   */
  template <typename Owner, typename Element, typename GEOIDITER>
  class geometry_element_iterator {
  public:
    using id_iterator_t = GEOIDITER;

    static_assert(has_geometry_id_iterator_tag<id_iterator_t>::value,
                  "template class for geometry_element_iterator must be a geometry iterator");

    using iterator = geometry_element_iterator<Owner, Element, id_iterator_t>; ///< this type

    /// @{
    /// @name Types mirrored from the ID iterator
    using LocalID_t = typename id_iterator_t::LocalID_t;
    using GeoID_t = typename id_iterator_t::GeoID_t;
    using ElementPtr_t = Element const*;
    /// @}

    /// @name Iterator traits
    /// @{
    using difference_type = std::ptrdiff_t;
    using value_type = Element;
    using reference = value_type const&;
    using pointer = value_type const*;
    using iterator_category = std::forward_iterator_tag;
    /// @}

    /// Default constructor; effect not defined: assign to it before using!
    geometry_element_iterator() = default;

    /// Constructor: points to the same element as the specified ID iterator.
    geometry_element_iterator(Owner const* owner, id_iterator_t const& iter)
      : geom{owner}, id_iter(iter)
    {}

    /**
     * @brief Returns the geometry element the iterator points to
     * @return a constant reference to the element the iterator points to
     * @throw cet::exception (category "geometry_iterator") if no valid
     *   geometry element is currently pointed by the iterator
     */
    reference operator*() const
    {
      ElementPtr_t ptr = get();
      if (ptr) return *ptr;
      throw cet::exception("geometry_iterator")
        << "iterator attempted to obtain geometry element " << std::string(ID());
    }

    /// Returns a pointer to the element the iterator points to (or nullptr)
    pointer operator->() const { return get(); }

    /// Prefix increment: returns this iterator pointing to the next element
    iterator& operator++()
    {
      ++id_iter;
      return *this;
    }

    /// Postfix increment: returns the current iterator, then increments it
    iterator operator++(int)
    {
      iterator old(*this);
      ++id_iter;
      return old;
    }

    /// Returns whether the iterator is pointing to a valid geometry element
    operator bool() const { return validElement(geom, *id_iter); }

    /// Returns a pointer to the geometry element, or nullptr if invalid
    ElementPtr_t get() const { return getElementPtr(geom, *id_iter); }

    /// Returns the ID of the pointed geometry element
    LocalID_t const& ID() const { return *id_iter; }

    auto id_iterator() const { return id_iter; }

  private:
    Owner const* geom;
    id_iterator_t id_iter;
  }; // class geometry_element_iterator<>

  // Element here supports types like CryostatGeo, etc.
  template <typename Owner, typename Element, typename IterationPolicy>
  using element_iterator_for =
    geometry_element_iterator<Owner, Element, id_iterator<typename Element::ID_t, IterationPolicy>>;

  template <typename Element>
  using element_sentinel_for = id_sentinel<typename Element::ID_t>;

  // Element iterator/sentinel comparisons
  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator==(geometry_element_iterator<Owner, Element, GEOIDITER> const& a,
                  geometry_element_iterator<Owner, Element, GEOIDITER> const& b)
  {
    return a.id_iterator() == b.id_iterator();
  }

  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator!=(geometry_element_iterator<Owner, Element, GEOIDITER> const& a,
                  geometry_element_iterator<Owner, Element, GEOIDITER> const& b)
  {
    return !(a == b);
  }

  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator==(geometry_element_iterator<Owner, Element, GEOIDITER> const& a,
                  id_sentinel<typename Element::ID_t> const& b)
  {
    return a.id_iterator() == b;
  }

  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator!=(geometry_element_iterator<Owner, Element, GEOIDITER> const& a,
                  id_sentinel<typename Element::ID_t> const& b)
  {
    return !(a == b);
  }

  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator==(id_sentinel<typename Element::ID_t> const& a,
                  geometry_element_iterator<Owner, Element, GEOIDITER> const& b)
  {
    return a == b.id_iterator();
  }

  template <typename Owner, typename Element, typename GEOIDITER>
  bool operator!=(id_sentinel<typename Element::ID_t> const& a,
                  geometry_element_iterator<Owner, Element, GEOIDITER> const& b)
  {
    return !(a == b);
  }

} // namespace geo::details

#endif // LARCOREALG_GEOMETRY_DETAILS_ELEMENT_ITERATORS_H
