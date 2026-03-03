#ifndef LARCOREALG_GEOMETRY_DETAILS_ID_ITERATORS_H
#define LARCOREALG_GEOMETRY_DETAILS_ID_ITERATORS_H

// LArSoft libraries
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "larcoreobj/SimpleTypesAndConstants/readout_types.h"

// C/C++ standard libraries
#include <iterator> // std::forward_iterator_tag
#include <string>
#include <utility>

namespace geo::details {

  template <typename ID>
  struct id_sentinel {
    ID id;
  };

  template <typename ID>
  id_sentinel(ID) -> id_sentinel<ID>;

  template <typename ID>
  std::ostream& operator<<(std::ostream& out, id_sentinel<ID> const& sentinel)
  {
    return out << "id_sentinel{ " << sentinel.id << " }";
  }

  /**
   * @brief Base forward iterator browsing all cryostat IDs in the detector
   * @tparam GEOID ID type to be used
   *
   * This iterator assumes that GEOID is derived from geo::CryostatID.
   * Note that no polymorphic behaviour is required, or expected, from GEOID.
   *
   * This iterator is designed to carry on, untouched, anything else that the
   * GEOID type defines beyond the required CryostatID data.
   */
  template <typename LocalID, typename GEOID, typename IterationPolicy>
  class id_iterator_base;

  template <typename GEOID, typename IterationPolicy>
  class id_iterator_base<CryostatID, GEOID, IterationPolicy> {
  public:
    struct GeometryIDIteratorTag {};
    using GeoID_t = GEOID; ///< type of the actual ID stored in the iterator

    using LocalID_t = CryostatID; ///< type of the ID we change
    using iterator = id_iterator_base<LocalID_t, GeoID_t, IterationPolicy>; ///< this iterator

    static_assert(std::is_base_of<LocalID_t, GEOID>{}, "template type GEOID is not a LocalID_t");

    /// @name Iterator traits
    /// @{
    using difference_type = std::ptrdiff_t;
    using value_type = LocalID_t;
    using reference = value_type const&;
    using pointer = value_type const*;
    using iterator_category = std::forward_iterator_tag;
    /// @}

    /// Default constructor; effect not defined: assign to it before using!
    id_iterator_base() = default;

    /// Constructor: points to the specified cryostat
    id_iterator_base(GeoID_t const& start_from, IterationPolicy policy)
      : id{start_from}, limit{policy.NSiblings(localID())}
    {}

    /// Returns the ID the iterator points to
    reference operator*() const { return localID(); }
    pointer operator->() const { return &localID(); }

    /// Prefix increment: returns this iterator pointing to the next cryostat
    iterator& operator++()
    {
      next();
      return *this;
    }

    /// Postfix increment: returns the current iterator, then increments it
    iterator operator++(int)
    {
      iterator old(*this);
      next();
      return old;
    }

    LocalID_t const& localID() const { return ID(); }

  protected:
    using ID_t = typename LocalID_t::CryostatID_t;

    /// Skips to the next cryostat
    void next()
    {
      if (at_end()) return;
      if (++local_index() < limit) return;
    }

    //@{
    /// Returns the actual type of ID we store
    GeoID_t const& ID() const { return id; }
    GeoID_t& ID() { return id; }
    //@}

    /// Returns whether this iterator has reached the end
    bool at_end() const { return local_index() == limit; }

  private:
    GeoID_t id{};                      ///< ID of the current cryostat
    ID_t limit = LocalID_t::InvalidID; ///< maximum number of cryostats

    //@{
    /// Returns the type of ID we act on
    LocalID_t& localID() { return ID(); }
    //@}

    //@{
    /// Returns the index (part if the ID) this iterator runs on
    ID_t const& local_index() const { return localID().deepestIndex(); }
    ID_t& local_index() { return localID().deepestIndex(); }
    //@}
  }; // class id_iterator_base<CryostatID>

  /**
   * @brief Base forward iterator browsing all TPC IDs in the detector
   * @tparam GEOID ID type to be used
   *
   * This iterator requires that GEOID is derived from geo::TPCID.
   * Note that no polymorphic behaviour is required, or expected, from GEOID.
   *
   * This iterator is designed to carry on, untouched, anything else that the
   * GEOID type defines beyond the required TPCID data.
   *
   * @note A number of "local" methods are overloaded: since there is no
   * polymorphism here and they are not virtual functions, these are designed
   * not to replace the inherited methods except within the non-inherited and
   * explicitly redefined methods.
   */
  template <typename LocalID, typename GEOID, typename IterationPolicy>
  class id_iterator_base
    : protected id_iterator_base<typename LocalID::ParentID_t, GEOID, IterationPolicy> {
    using upper_iterator = id_iterator_base<typename LocalID::ParentID_t, GEOID, IterationPolicy>;

  public:
    struct GeometryIDIteratorTag {};
    using GeoID_t = typename upper_iterator::GeoID_t;

    using LocalID_t = LocalID; ///< type of the ID we change
    static_assert(std::is_base_of<LocalID_t, GEOID>{}, "template type GEOID is not a LocalID_t");

    using iterator =
      id_iterator_base<LocalID_t, GeoID_t, IterationPolicy>; ///< type of this iterator

    /// @name Iterator traits
    /// @{
    using difference_type = std::ptrdiff_t;
    using value_type = LocalID_t;
    using reference = value_type const&;
    using pointer = value_type const*;
    using iterator_category = std::forward_iterator_tag;
    /// @}

    /// Default constructor; effect not defined: assign to it before using!
    id_iterator_base() = default;

    /// Constructor: points to the specified element
    id_iterator_base(GeoID_t const& start_from, IterationPolicy itPolicy)
      : upper_iterator{start_from, itPolicy}, policy{itPolicy}, limit(policy.NSiblings(localID()))
    {}

    /// Returns the element ID the iterator points to
    reference operator*() const { return localID(); }
    pointer operator->() const { return &localID(); }

    /// Prefix increment: returns this iterator pointing to the next element
    iterator& operator++()
    {
      next();
      return *this;
    }

    /// Postfix increment: returns the current iterator, then increments it
    iterator operator++(int)
    {
      iterator old(*this);
      next();
      return old;
    }

    LocalID_t const& localID() const { return upper_iterator::ID(); }

  protected:
    /// Skips to the next element
    void next()
    {
      // if at end (checked in the inherited context), do nothing
      if (upper_iterator::at_end()) return;

      // if after incrementing we haven't reached the limit, we are done
      if (++local_index() < limit) return;

      // we reached the end of the current elements list, we need to escalate:
      // - go to the next parent; if that becomes invalid, too bad, but we go on
      upper_iterator::next();
      // - set the index to the first element of the new parent
      local_index() = 0;
      // - update how many elements there are
      //   (expect 0 if it is now at_end() -- and it does not even matter)
      limit = policy.NSiblings(localID());
    }

  private:
    IterationPolicy policy;

    using ID_t = std::decay_t<
      decltype(std::declval<LocalID_t>().deepestIndex())>; ///< specific type for element ID

    /// maximum number of elements in the current cryostat
    ID_t limit = LocalID_t::InvalidID;

    /// Returns the type of ID we act on
    LocalID_t& localID() { return upper_iterator::ID(); }

    /// Returns the index (part if the ID) this iterator runs on
    ID_t const& local_index() const { return localID().deepestIndex(); }
    ID_t& local_index() { return localID().deepestIndex(); }
  }; // class id_iterator_base

  template <typename LocalID, typename SiblingPolicy>
  using id_iterator = id_iterator_base<LocalID, LocalID, SiblingPolicy>;

  template <typename LocalID, typename SiblingPolicy>
  auto iterator_for(LocalID const& id, SiblingPolicy const& siblingPolicy)
  {
    return id_iterator<LocalID, SiblingPolicy>(id, siblingPolicy);
  }

  /// Stream output for all geometry ID iterator types: prints the pointed ID.
  template <typename T, typename = void>
  struct has_geometry_id_iterator_tag : std::false_type {};

  template <typename T>
  struct has_geometry_id_iterator_tag<T, std::void_t<typename T::GeometryIDIteratorTag>>
    : std::true_type {};

  template <typename GEOIT>
  std::enable_if_t<has_geometry_id_iterator_tag<GEOIT>::value, std::ostream&> operator<<(
    std::ostream& out,
    GEOIT const& it)
  {
    return out << "geometry_iterator{ " << *it << " }";
  }

  template <typename ID, typename Policy>
  struct supported {
    static constexpr bool check(unsigned int (Policy::*)(ID const&) const) { return true; }
    static constexpr bool check(...) { return false; }
  };

  /// Sets the ID to the ID after the specified one.
  /// @return whether the ID is actually valid (validity flag is also set)
  template <typename ID, typename IterationPolicy>
  void IncrementID(ID& id, IterationPolicy const& policy)
  {
    static_assert(supported<ID, IterationPolicy>::check(&IterationPolicy::NSiblings));

    if (++id.deepestIndex() < policy.NSiblings(id)) return;
    if constexpr (has_parent<ID>::value) {
      // go to next parent element and reset index
      id.deepestIndex() = 0;
      IncrementID(id.parentID(), policy);
    }
  }

  /**
   * @brief Returns the ID next to the specified one.
   * @tparam GeoID type of the ID to be returned
   * @param id the element ID to be incremented
   * @return ID of the next subelement after `id`
   */
  template <typename GeoID, typename IterationPolicy>
  GeoID GetNextID(GeoID const& id, IterationPolicy const& policy)
  {
    auto nextID(id);
    IncrementID(nextID, policy);
    return nextID;
  }

  // ID iterator/sentinel comparisons
  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator==(id_iterator_base<LocalID, GeoID, IterationPolicy> const& a,
                  id_iterator_base<LocalID, GeoID, IterationPolicy> const& b)
  {
    return a.localID() == b.localID();
  }

  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator!=(id_iterator_base<LocalID, GeoID, IterationPolicy> const& a,
                  id_iterator_base<LocalID, GeoID, IterationPolicy> const& b)
  {
    return !(a == b);
  }

  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator==(id_iterator_base<LocalID, GeoID, IterationPolicy> const& a,
                  id_sentinel<LocalID> const& b)
  {
    return a.localID() == b.id;
  }

  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator!=(id_iterator_base<LocalID, GeoID, IterationPolicy> const& a,
                  id_sentinel<LocalID> const& b)
  {
    return !(a == b);
  }

  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator==(id_sentinel<LocalID> const& a,
                  id_iterator_base<LocalID, GeoID, IterationPolicy> const& b)
  {
    return a.id == b.localID();
  }

  template <typename LocalID, typename GeoID, typename IterationPolicy>
  bool operator!=(id_sentinel<LocalID> const& a,
                  id_iterator_base<LocalID, GeoID, IterationPolicy> const& b)
  {
    return !(a == b);
  }

  template <typename LocalID>
  bool operator==(id_sentinel<LocalID> const& a, id_sentinel<LocalID> const& b)
  {
    return a.id == b.id;
  }

  template <typename LocalID>
  bool operator!=(id_sentinel<LocalID> const& a, id_sentinel<LocalID> const& b)
  {
    return !(a == b);
  }

} // namespace geo::details

#endif // LARCOREALG_GEOMETRY_DETAILS_ID_ITERATORS_H
