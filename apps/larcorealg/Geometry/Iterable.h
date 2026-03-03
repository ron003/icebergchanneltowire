#ifndef LARCOREALG_GEOMETRY_ITERABLE_H
#define LARCOREALG_GEOMETRY_ITERABLE_H

// LArSoft libraries
#include "larcorealg/Geometry/details/element_iterators.h"
#include "larcorealg/Geometry/details/id_iterators.h"
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

// External libraries
//#include "range/v3/view.hpp"

// C/C++ standard libraries
#include <type_traits>
#include <utility>

namespace geo {

  struct void_tag {};

  namespace details {
    template <typename T, typename = void>
    struct IDIteratorValueType {
      static_assert(std::is_base_of_v<CryostatID, T>);
      using type = T;
    };

    template <typename T>
    struct IDIteratorValueType<T, std::void_t<typename T::ID_t>> {
      static_assert(!std::is_base_of_v<CryostatID, T>);
      using type = typename T::ID_t;
    };

    template <typename T, typename Policy, typename Transform, typename = void>
    struct IteratorFor {
      using ID = typename IDIteratorValueType<T>::type;
      using type = decltype(iterator_for(ID{}, std::declval<Policy>()));
    };

    template <typename T, typename Policy, typename Transform>
    struct IteratorFor<T,
                       Policy,
                       Transform,
                       std::enable_if_t<!std::is_same_v<Transform, void_tag> and
                                        !std::is_base_of_v<CryostatID, T>>> {
      using ID = typename IDIteratorValueType<T>::type;
      using type = decltype(std::declval<Transform>().template transform<T>(
        iterator_for(ID{}, std::declval<Policy>())));
    };

    template <typename Begin, typename End>
    using RangeType = decltype(ranges::make_subrange(std::declval<Begin>(), std::declval<End>()));
  }

  template <typename IterationPolicy, typename Transform = void_tag>
  class Iterable {
    template <typename T>
    using id_sentinel_for = details::id_sentinel<typename details::IDIteratorValueType<T>::type>;

    template <typename T>
    using iterator_type = typename details::IteratorFor<T, IterationPolicy, Transform>::type;

  public:
    explicit Iterable(IterationPolicy const& policy) : fPolicy{policy} {}

    Iterable(IterationPolicy const& policy, Transform const& transform)
      : fPolicy{policy}, fTransform{transform}
    {}

    // FIXME: Need Doxygen comments
    template <typename T>
    iterator_type<T> begin() const
    {
      using namespace details;
      using ID = typename IDIteratorValueType<T>::type;
      auto const iterator = iterator_for(ID::first(), fPolicy);
      if constexpr (std::is_base_of<CryostatID, T>{}) { return iterator; }
      else {
        static_assert(!std::is_same_v<Transform, void_tag>);
        return fTransform.template transform<T>(iterator);
      }
    }

    template <typename T, typename BaseID>
    iterator_type<T> begin(BaseID const& id) const
    {
      using namespace details;
      using ID = typename IDIteratorValueType<T>::type;
      static_assert(is_base_of_strict<BaseID, ID>);
      auto const iterator = iterator_for(ID::first(id), fPolicy);
      if constexpr (std::is_base_of<CryostatID, T>{}) { return iterator; }
      else {
        static_assert(!std::is_same_v<Transform, void_tag>);
        return fTransform.template transform<T>(iterator);
      }
    }

    template <typename T>
    id_sentinel_for<T> end() const
    {
      using namespace details;
      using ID = typename IDIteratorValueType<T>::type;
      static_assert(std::is_base_of<CryostatID, ID>{});
      return {fPolicy.template GetEndID<ID>()};
    }

    template <typename T, typename BaseID>
    id_sentinel_for<T> end(BaseID const& id) const
    {
      using namespace details;
      using ID = typename IDIteratorValueType<T>::type;
      static_assert(std::is_base_of<CryostatID, ID>{});
      static_assert(is_base_of_strict<BaseID, ID>);
      return {fPolicy.template GetEndID<ID>(id)};
    }

    template <typename T>
    using range_type = details::RangeType<iterator_type<T>, id_sentinel_for<T>>;

    template <typename T>
    range_type<T> Iterate() const
    {
      return ranges::make_subrange(begin<T>(), end<T>());
    }

    template <typename T, typename ID>
    range_type<T> Iterate(ID const& id) const
    {
      return ranges::make_subrange(begin<T>(id), end<T>(id));
    }

  private:
    IterationPolicy fPolicy;
    std::conditional_t<std::is_same_v<Transform, void>, void_tag, Transform> fTransform;
  }; // Iterable

} // namespace geo

#endif // LARCOREALG_GEOMETRY_ITERABLE_H
