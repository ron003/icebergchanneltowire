/**
 * @file   larcorealg/Geometry/geo_vectors_utils.h
 * @brief  Utilities to extend the interface of geometry vectors.
 * @ingroup Geometry
 *
 * This library provides facilities that can be used for both LArSoft geometry
 * vectors (`geo_vectors.h`) and ROOT `TVector3` and related, with the same
 * interface.
 *
 * This library depends on ROOT GenVector.
 * In the CET link list in `CMakeLists.txt`, link to `${ROOT_GENVECTOR}`.
 *
 */

#ifndef LARCOREALG_GEOMETRY_GEO_VECTORS_UTILS_H
#define LARCOREALG_GEOMETRY_GEO_VECTORS_UTILS_H

// LArSoft libraries
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_vectors.h"

// ROOT libraries
#if 0
#include "Math/GenVector/DisplacementVector2D.h"
#include "Math/GenVector/DisplacementVector3D.h"
#include "Math/GenVector/LorentzVector.h"
#include "Math/GenVector/PositionVector2D.h"
#include "Math/GenVector/PositionVector3D.h"
#endif // if 0

// C/C++ standard library
#include <array>
#include <cassert>
#include <functional>  // std::mem_fn()
#include <iterator>    // std::back_inserter()
#include <type_traits> // std::declval(), std::is_same<>, ...
#include <vector>

namespace lar::util::simple_geo {
  template <typename T>
  struct Point2D;
  template <typename T>
  struct Point3D;
}

namespace geo {

  /**
   * @brief Utilities to manipulate geometry vectors.
   * @ingroup Geometry
   *
   * The utilities include generic vector interface facilities allowing to
   * use different vector types via templates.
   */
  namespace vect {

    //--------------------------------------------------------------------------
    namespace details {
      //------------------------------------------------------------------------
      template <typename Op, typename... T>
      struct AccumulateImpl;

      template <typename Op, typename First, typename Second, typename... Others>
      struct AccumulateImpl<Op, First, Second, Others...> {
        static auto compute(Op op, First&& a, Second&& b, Others&&... others) -> decltype(auto)
        {
          return op(a,
                    AccumulateImpl<Op, Second, Others...>::compute(
                      op, std::forward<Second>(b), std::forward<Others>(others)...));
        }
      }; // AccumulateImpl<>

      template <typename Op, typename T>
      struct AccumulateImpl<Op, T> {
        static auto compute(Op, T&& v) -> decltype(auto) { return std::forward<T>(v); }
      };

      template <typename Op, typename... T>
      auto extended_accumulate(Op op, T&&... args)
      {
        return AccumulateImpl<Op, T...>::compute(op, std::forward<T>(args)...);
      }

      template <typename... T>
      auto extended_and(T... args) -> decltype(auto)
      {
        auto and_op = [](auto&& a, auto&& b) { return a && b; };
        return extended_accumulate(and_op, std::forward<T>(args)...);
      }

      //------------------------------------------------------------------------
      //
      // These two pages of metaprogramming madness are aimed to have objects
      // that can provide a uniform coordinate get/set for any type of vector,
      // with the minimal run-time overhead.
      // Complications arise from the fact that TVector3 setter returns void,
      // while GenVector vectors return a reference to the vector itself
      // (hence the need of the additional SetterResult template parameter,
      // and of quite some code to autodetect its value).
      // The makeXxxx() functions are aimed to enclose the additional
      // autodetection overlay, which should become unnecessary with C++17.
      //

      template <typename Vector>
      struct VectorScalar {
        using type = typename Vector::Scalar;
      };

      template <typename Vector>
      using VectorScalar_t = typename VectorScalar<Vector>::type;

      //------------------------------------------------------------------------
      template <typename Vector>
      struct HasGetter {
      private:
        template <typename Class>
        static constexpr bool TestX(decltype(std::declval<Class>().X())*)
        {
          return true;
        }
        template <typename Class>
        static constexpr bool TestX(...)
        {
          return false;
        }
        template <typename Class>
        static constexpr bool TestY(decltype(std::declval<Class>().Y())*)
        {
          return true;
        }
        template <typename Class>
        static constexpr bool TestY(...)
        {
          return false;
        }
        template <typename Class>
        static constexpr bool TestZ(decltype(std::declval<Class>().Z())*)
        {
          return true;
        }
        template <typename Class>
        static constexpr bool TestZ(...)
        {
          return false;
        }
        template <typename Class>
        static constexpr bool TestT(decltype(std::declval<Class>().T())*)
        {
          return true;
        }
        template <typename Class>
        static constexpr bool TestT(...)
        {
          return false;
        }

      public:
        static constexpr bool X = TestX<Vector>(nullptr);
        static constexpr bool Y = TestY<Vector>(nullptr);
        static constexpr bool Z = TestZ<Vector>(nullptr);
        static constexpr bool T = TestT<Vector>(nullptr);
      }; // struct HasGetter<>

      template <typename Vector>
      constexpr bool HasX()
      {
        return HasGetter<Vector>::X;
      }
      template <typename Vector>
      constexpr bool HasY()
      {
        return HasGetter<Vector>::Y;
      }
      template <typename Vector>
      constexpr bool HasZ()
      {
        return HasGetter<Vector>::Z;
      }
      template <typename Vector>
      constexpr bool HasT()
      {
        return HasGetter<Vector>::T;
      }

      template <typename Vector, typename = void>
      struct DimensionImpl;

      template <typename Vector>
      constexpr unsigned int dimension()
      {
        return DimensionImpl<Vector>::value;
      }

      /// A STL array suitable to contain all coordinate values of a `Vector`.
      template <typename Vector>
      using CoordinateArray_t = std::array<VectorScalar_t<Vector>, dimension<Vector>()>;

      template <typename T>
      struct MemberFuncReturnType {
        // with C++17 `result_type` is deprecated,
        // and a different way will be needed
        using type = typename decltype(std::mem_fn(std::declval<T>()))::result_type;
      }; // MemberFuncReturnType

      template <typename T>
      using MemberFuncReturn_t = typename MemberFuncReturnType<T>::type;

      template <typename T>
      struct MemberFuncClassType;

      template <typename Class, typename Func>
      struct MemberFuncClassType<Func Class::*> {
        using type = Class;
      };

      template <typename T>
      using MemberFuncClass_t = typename MemberFuncClassType<T>::type;

      template <typename Vector, typename SetterType = void>
      struct BaseCoordTypes;

      template <typename Vector>
      struct BaseCoordTypes<Vector, void> {
        using Vector_t = std::decay_t<Vector>;
        using Scalar_t = VectorScalar_t<Vector>;
        using Getter_t = Scalar_t (Vector_t::*)() const;
      }; // struct BaseCoordTypes<void>

      template <typename Vector, typename SetterType>
      struct BaseCoordTypes {
      private:
        using BaseTypes_t = BaseCoordTypes<Vector, void>;

      public:
        using Scalar_t = typename BaseTypes_t::Scalar_t;
        using Vector_t = typename BaseTypes_t::Vector_t;
        using Getter_t = typename BaseTypes_t::Getter_t;
        using Setter_t = SetterType;
        using SetterReturn_t = MemberFuncReturn_t<Setter_t>;
        static_assert(std::is_same<Setter_t, SetterReturn_t (Vector_t::*)(Scalar_t)>(),
                      "Invalid setter type");
      }; // struct BaseCoordTypes<>

      template <typename Vector>
      struct CoordGetterTraits {
      private:
        using BaseTypes_t = BaseCoordTypes<Vector>;

      public:
        using Vector_t = typename BaseTypes_t::Vector_t;
        using Scalar_t = typename BaseTypes_t::Scalar_t;
        using Getter_t = typename BaseTypes_t::Getter_t;
      }; // struct CoordGetterTraits

      /// Helper class for read of a single vector coordinate.
      template <typename Vector>
      class CoordGetter {
        using Traits_t = CoordGetterTraits<Vector>;

      public:
        using Vector_t = typename Traits_t::Vector_t;
        using Scalar_t = typename Traits_t::Scalar_t;
        using Getter_t = typename Traits_t::Getter_t;

        /// Constructor: sets getter and setter functions.
        constexpr CoordGetter(Getter_t getter) : fGetter(getter) {}

        /// Returns the value of the bound coordinate.
        Scalar_t operator()(Vector_t const& v) const { return get(v); }

        /// Returns the value of the bound coordinate.
        Scalar_t get(Vector_t const& v) const { return (v.*fGetter)(); }

      private:
        Getter_t fGetter; ///< Member function returning the coordinate value.

      }; // class CoordGetter<>

      template <typename Getter>
      constexpr auto makeCoordReader(Getter getter)
      {
        using Vector_t = std::remove_reference_t<MemberFuncClass_t<Getter>>;
        return CoordGetter<Vector_t>{getter};
      }

      template <typename Vector, typename SetterType>
      struct CoordManagerTraits {
      private:
        using BaseTypes_t = BaseCoordTypes<Vector, SetterType>;

      public:
        using Vector_t = typename BaseTypes_t::Vector_t;
        using Scalar_t = typename BaseTypes_t::Scalar_t;
        using Getter_t = typename BaseTypes_t::Getter_t;
        using Setter_t = typename BaseTypes_t::Setter_t;
      }; // struct VectorCoordManagerTraits<>

      /// Helper class for read/write of a single vector coordinate.
      template <typename Vector, typename SetterType>
      class CoordManager : public CoordGetter<Vector> {
        using Base_t = CoordGetter<Vector>;
        using Traits_t = CoordManagerTraits<Vector, SetterType>;

      public:
        using Vector_t = typename Traits_t::Vector_t; // this is not constant
        using Scalar_t = typename Traits_t::Scalar_t;
        using Getter_t = typename Traits_t::Getter_t;
        using Setter_t = typename Traits_t::Setter_t;

        /// Constructor: sets getter and setter functions.
        constexpr CoordManager(Getter_t getter, Setter_t setter) : Base_t(getter), fSetter(setter)
        {}

        /// Setter: assigns a value to the bound coordinate of specified vector.
        void operator()(Vector_t& v, Scalar_t c) const { set(v, c); }

        /// Setter: assigns a value to the bound coordinate of specified vector.
        void set(Vector_t& v, Scalar_t c) const { (v.*fSetter)(c); }

        /// Increments the coordinate by the specified amount.
        void incr(Vector_t& v, Scalar_t c) const { set(v, Base_t::get(v) + c); }

        /// Decrements the coordinate by the specified amount.
        void decr(Vector_t& v, Scalar_t c) const { set(v, Base_t::get(v) - c); }

        /// Multiplies the coordinate by the specified amount.
        void mult(Vector_t& v, Scalar_t f) const { set(v, Base_t::get(v) * f); }

        /// Divides the coordinate by the specified amount.
        void div(Vector_t& v, Scalar_t f) const { set(v, Base_t::get(v) / f); }

      private:
        Setter_t fSetter; ///< Member function setting the coordinate value.
      };                  // class CoordManager<>

      template <typename Getter, typename Setter>
      constexpr auto makeCoordManager(Getter getter, Setter setter)
      {
        using Vector_t = std::remove_reference_t<MemberFuncClass_t<Getter>>;
        return CoordManager<Vector_t, Setter>{getter, setter};
      }

      template <typename CoordHelper, typename StoredVector>
      class BoundCoordGetter {

      public:
        using Stored_t = StoredVector;

        using CoordHelper_t = CoordHelper;
        using Vector_t = typename CoordHelper_t::Vector_t;
        using Scalar_t = typename CoordHelper_t::Scalar_t;
        using Getter_t = typename CoordHelper_t::Getter_t;

        /// Constructor: manage the specified coordinate of specified vector.
        BoundCoordGetter(Stored_t& v, CoordHelper_t coordManager) : fCoord(coordManager), fVector(v)
        {}

        /// Constructor: manage the specified vector with specified methods.
        BoundCoordGetter(Stored_t& v, Getter_t getter) : fCoord(getter), fVector(v) {}

        /// Returns the value of the bound coordinate.
        Scalar_t get() const { return manager().get(vector()); }

        /// Returns the value of the bound coordinate.
        Scalar_t operator()() const { return get(); }

        /// Returns the value of the bound coordinate.
        operator Scalar_t() const { return manager().get(vector()); }

      protected:
        CoordHelper_t const& manager() const { return fCoord; }
        Stored_t& vector() const { return fVector; }

      private:
        CoordHelper_t fCoord; ///< Helper to manage a specific coordinate.
        Stored_t& fVector;    ///< The vector to manage the coordinate of.
      };                      // class VectorCoordGetter<>

      template <typename CoordHelper, typename StoredVector>
      class BoundCoordManager : public BoundCoordGetter<CoordHelper, StoredVector> {
        using Base_t = BoundCoordGetter<CoordHelper, StoredVector>;

      public:
        using typename Base_t::Stored_t;

        using CoordHelper_t = CoordHelper;
        using Vector_t = typename CoordHelper_t::Vector_t;
        using Scalar_t = typename CoordHelper_t::Scalar_t;
        using Getter_t = typename CoordHelper_t::Getter_t;
        using Setter_t = typename CoordHelper_t::Setter_t;

        /// Constructor: manage the specified coordinate of specified vector.
        BoundCoordManager(Stored_t& v, CoordHelper_t coordManager) : Base_t(v, coordManager) {}

        /// Constructor: manage the specified vector with specified methods.
        BoundCoordManager(Stored_t& v, Getter_t getter, Setter_t setter)
          : Base_t(v, CoordHelper_t(getter, setter))
        {}

        /// Setter: assigns a value to the bound coordinate of specified vector.
        BoundCoordManager& operator=(Scalar_t c)
        {
          Base_t::manager().set(Base_t::vector(), c);
          return *this;
        }

        /// Increments by the specified amount.
        BoundCoordManager& operator+=(Scalar_t c)
        {
          Base_t::manager().incr(Base_t::vector(), c);
          return *this;
        }

        /// Decrements by the specified amount.
        BoundCoordManager& operator-=(Scalar_t c)
        {
          Base_t::manager().decr(Base_t::vector(), c);
          return *this;
        }

        /// Multiplies by the specified amount.
        BoundCoordManager& operator*=(Scalar_t f)
        {
          Base_t::manager().mult(Base_t::vector(), f);
          return *this;
        }

        /// Divides by the specified amount.
        BoundCoordManager& operator/=(Scalar_t f)
        {
          Base_t::manager().div(Base_t::vector(), f);
          return *this;
        }

      }; // class BoundCoordManager

      //------------------------------------------------------------------------

    } // namespace details

    // BEGIN Geometry group ------------------------------------------------------
    /// @ingroup Geometry
    /// @{

    /// Convenience utilities not directly related to vectors.
    namespace extra {

      /// Returns `value`, rounded to 0 if closer than `tol`.
      template <typename T>
      constexpr T roundValue0(T value, T tol)
      {
        return (std::abs(value) < tol) ? T{} : value;
      } // roundValue0()

      /// Returns `value`, rounded to 0, -1 or +1 if closer than `tol`.
      template <typename T>
      constexpr T roundValue01(T value, T tol)
      {
        if (std::abs(value) < tol) return 0.;
        if (std::abs(std::abs(value) - 1.) < tol) return (value > 0.) ? 1. : -1.;
        return value;
      } // roundValue01()

    } // namespace extra

    // --- BEGIN Vector coordinate access abstraction --------------------------
    /// @{
    /**
     * @name Vector coordinate access abstraction
     *
     * This group of utilities provides a common interface for tasks involving
     * geometry vectors, which may have different interface.
     * An example of that is the access of coordinates by an index: it is
     * supported (and "slow") in `TVector3` for read/write access, while in
     * GenVector it is not supported (and given that the internal representation
     * might be not cartesian, it's not surprising). We provide utilities which
     * fill the gaps, relying on some looser requirements.
     *
     *
     * Coordinate managers
     * ====================
     *
     * A "coordinate manager" is an object handling a specific coordinate out of
     * a specific vector type; i.e., a coordinate manager object will manage for
     * its entire lifetime the same coordinate, e.g. _x_ or _z_.
     * A coordinate manager can be:
     * - bound to a vector object: that manager handles exclusively its managed
     *   coordinate for that vector object;
     * - unbound: such a manager can handle the managed coordinate of any
     *   vector, which can be passed as an argument; or the unbound manager can
     *   be used to create a bound one.
     *
     * Two types of managers are available:
     * - reader, accesses the coordinate but can't modify it
     * - manager, requires to be bound to a mutable vector and can assign and
     *   modify the coordinate via selected operations
     *
     * Note that a coordinate is never returned as a reference, either mutable
     * or constant.
     *
     *
     * Handling a coordinate of a vector object: bound managers
     * ---------------------------------------------------------
     *
     * A bound coordinate manager can be created directly:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * geo::Point_t p { 1.0, 2.0, 3.0 };
     *
     * auto px = geo::vect::Xcoord(p);
     * std::cout << p << " has x=" << px() << std::endl;
     * px += 5.0;
     * std::cout << p << " has now x=" << px() << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will return something along the line of
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (1,2,3) has x=1
     * (6,2,3) has now x=6
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Functions `Xcoord()`, `Ycoord()`, `Zcoord()` and `Tcoord()` (in namespace
     * `geo::vect`) are available for the supporting vector types.
     *
     * If access by numeric index is necessary, `coord()` can be used instead:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * geo::Vector_t const v { 1.0, 2.0, 3.0 };
     *
     * for (unsigned c = 0; c < 3; ++c) {
     *   auto vc = geo::vect::coord(v, c);
     *   std::cout << v << "[" << c << "]=" << vc() << std::endl;
     * }
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (note that for this example we have implicitly obtained a coordinate
     * reader instead of a full coordinate manager because `v` is constant).
     * This will print:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * v[0]=1
     * v[1]=2
     * v[2]=3
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * If there are more vectors to access the same coordinate of, it's better
     * to use unbound managers (see below).
     *
     *
     * Handling a coordinate for any vector object
     * --------------------------------------------
     *
     * Unbound coordinate managers (and readers) can't operate directly on
     * vectors but they need to be bound to one. Binding produces a new bound
     * manager, leaving the unbound manager untouched.
     * Binding is done with `geo::vect::bindCoord()`. For example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * geo::Point_t A { 1.0, 2.0, 3.0 };
     * auto Ax = geo::vect::bindCoord(A, YcoordManager<geo::Point_t>);
     * std::cout << A << " has y=" << Ax << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * should produce an output like
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (1,2,3) has y=2
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * In the example, `YcoordManager` is a template coordinate manager.
     * There are managers available for `X`, `Y`, `Z` and `T` coordinates, and
     * each one can deal only with a specific vector type; also, specifying a
     * non-constant vector type will deliver a full manager, which can't operate
     * on constant vectors.
     * The unbound coordinate managers are not as useful, but a possible use is
     * for loops on coordinates from multiple vectors:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * geo::Point_t A { 1.0, 2.0, 3.0 }, geo::Point_t B {5.0, 7.0, 9.0 };
     * for (unsigned c = 0; c < 3; ++c) {
     *   auto coordMan = geo::vect::coordManager(c);
     *   auto Ac = geo::vect::bindCoord(A, coordMan);
     *   auto Bc = geo::vect::bindCoord(B, coordMan);
     *   std::cout << (Bc() - Ac() * 2.0) << std::endl;
     * } // for
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * which will emit
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * 3
     * 3
     * 3
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * This is marginally faster than the same code with
     * `geo::vect::bindCoord()` call replaced by `geo::vect::coord()`. More
     * convenient still, if the coordinates are treated all just the same and
     * `c` is not needed (as above):
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * geo::Point_t A { 1.0, 2.0, 3.0 }, geo::Point_t B {5.0, 7.0, 9.0 };
     * for (auto coordMan: geo::vect::coordManagers<geo::Point_t const>()) {
     *   auto Ac = geo::vect::bindCoord(A, coordMan);
     *   auto Bc = geo::vect::bindCoord(B, coordMan);
     *   std::cout << (Bc() - Ac() * 2.0) << std::endl;
     * } // for
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *
     *
     * Conversion between vector types
     * ================================
     *
     * A convenience function `convertTo()` is provided to convert a vector into
     * another of a different type (for example, from `TVector3` to
     * `geo::Vector_t`).
     *
     *
     * Vector requirements
     * ====================
     *
     * So far, the requirements for this set of utilities are the following.
     * The vector type must support:
     * - a cartesian coordinate constructor: `Vector v { 1.0, 2.0, 3.0 };`
     * - accessor methods named after the name of the coordinate, acting on
     *   constant vectors, taking no arguments, and returning a copy of the
     *   coordinate value, e.g. `double X() const`
     * - coordinate assignment methods named `SetC`, where `C` is the name of
     *   each coordinate, after the name of the coordinate, with a single
     *   argument; the return type is not prescribed; e.g. `void SetY(double)`
     * - the coordinate names must be `X` and `Y` for 2D vectors, plus `Z` for
     *   3D vectors and `T` for 4D vectors (metric is irrelevant here)
     *
     */

    //@{
    /// Returns the dimension of the specified vector type.
    template <typename Vector>
    constexpr unsigned int dimension()
    {
      return details::dimension<Vector>();
    }
    template <typename Vector>
    constexpr unsigned int dimension(Vector&&)
    {
      return dimension<Vector>();
    }
    //@}

    //@{
    /// Returns a sequence of indices valid for a vector of the specified type.
    template <typename Vector>
    constexpr std::array<std::size_t, vect::dimension<Vector>()> indices();
    template <typename Vector>
    constexpr auto indices(Vector const&) -> decltype(indices<Vector>());
    //@}

    /// Type of coordinate of the specified vector type.
    template <typename Vector>
    using coordinate_t = details::VectorScalar_t<Vector>;

    /**
     * @brief Creates a `Vector` object with coordinates from `coords`.
     * @tparam Vector the type of vector to be created
     * @tparam Coords type of object holding the value of the needed coordinates
     * @param coords object holding the value of the needed coordinates
     * @return a newly created `Vector` object with coordinates from `coords`
     * @see `geo::vect::fillCoords()`
     *
     * To create a vector of dimension _N_, the first _N_ values are extracted
     * from `coords` using `Coords::operator[](std::size_t)`. For example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * constexpr std::array<float, 5U> data { 2.0, 5.0, 7.0, 11.0, 15.5 };
     * constexpr auto p = geo::vect::makeFromCoords<geo::Point_t>(data);
     * auto v = geo::vect::makeFromCoords<geo::Vector_t>(data.data() + 1);
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will set `p` as `constexpr geo::Point_t {2.0, 5.0, 7.0 }`, ignoring the
     * additional data. Likewise, it will set `v` to
     * `geo::Vector_t{ 5.0, 7.0, 11.0 }`. In both cases, the coordinates are
     * implicitly converted from `float` into the scalar type of the target
     * vectors (in both cases, `double`).
     */
    template <typename Vector, typename Coords>
    constexpr Vector makeFromCoords(Coords&& coords);

    /**
     * @brief Fills a coordinate array with the coordinates of a vector.
     * @tparam Vector type of vector being copied from
     * @tparam Coords type of coordinate array to be filled
     * @param src the vector to read the coordinates from
     * @param dest the array to be filled
     * @return the number of coordinates filled (that's `Vector`'s dimension)
     * @see `geo::vect::makeFromCoords()`
     *
     * The `Coords` array type is expected to provide a indexing operator
     * returning a r-value, that is `coords[i]` can be assigned to.
     */
    template <typename Vector, typename Coords>
    unsigned int fillCoords(Coords& dest, Vector const& src);

    /// Type of a coordinate reader for a vector type.
    template <typename Vector>
    using CoordReader_t = decltype(details::makeCoordReader(&Vector::X));

    /// Type of a coordinate manager for a vector type.
    template <typename Vector>
    using CoordManager_t = decltype(details::makeCoordManager(&Vector::X, &Vector::SetX));

    /**
     * @brief Object that can be bound to a vector to manage its X coordinate.
     * @tparam Vector type of vector to get a manager for
     *                (mutable type required)
     *
     * The manager exposes a read/write interface.
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * // mutable vectors get a full-featured "manager":
     * geo::Point_t p { 1.0, 2.0, 3.0 };
     * auto px
     *   = geo::vect::bindCoord(p, geo::vect::XcoordManager<geo::Point_t>);
     * px *= 5.0;
     * std::cout << p << " has now x=" << px() << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will print something like:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (5,2,3) has now x=5
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Note that the use in the example, `Xcoord()` is preferred.
     */
    template <typename Vector>
    static constexpr auto XcoordManager = details::makeCoordManager(&Vector::X, &Vector::SetX);

    /**
     * @brief Object that can be bound to a vector to access its X coordinate.
     * @tparam Vector type of vector to get a manager for
     *                (constant type required)
     *
     * The manager exposes a read-only interface.
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * // constant vectors get a "reader" (read-only manager):
     * geo::Vector_t v { 1.0, 2.0, 3.0 };
     *
     * auto vx = geo::vect::bindCoord
     *   (v, geo::vect::XcoordManager<geo::Vector_t const>);
     * std::cout << v << " has x=" << vx() << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will print something like:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (1,2,3) has x=1
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Note that the use in the example, `Xcoord()` is preferred.
     */
    template <typename Vector>
    static constexpr auto XcoordManager<Vector const> = details::makeCoordReader(&Vector::X);

    /// An object that can be bound to a vector to manage its Y coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto const YcoordManager =
      details::makeCoordManager(&Vector::Y, &Vector::SetY);

    /// An object that can be bound to a vector to manage its Y coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto YcoordManager<Vector const> = details::makeCoordReader(&Vector::Y);

    /// An object that can be bound to a vector to manage its Z coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto ZcoordManager = details::makeCoordManager(&Vector::Z, &Vector::SetZ);

    /// An object that can be bound to a vector to manage its Z coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto ZcoordManager<Vector const> = details::makeCoordReader(&Vector::Z);

    /// An object that can be bound to a vector to manage its T coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto TcoordManager = details::makeCoordManager(&Vector::T, &Vector::SetT);

    /// An object that can be bound to a vector to manage its T coordinate.
    /// @see `geo::vect::XcoordManager`
    template <typename Vector>
    static constexpr auto TcoordManager<Vector const> = details::makeCoordReader(&Vector::T);

    /**
     * @brief Returns an object that can be bound to a vector to manage one of
     *        its coordinates.
     * @tparam Vector type of vector to get a manager for (constantness matters)
     * @param n index of the coordinate (`0`: X, `1`: Y, `2`: Z, `3`: T)
     * @return a coordinate manager, undefined if index is invalid
     *
     * Index `n` is assumed to be smaller than the dimension of the vector.
     * The manager returned for a mutable vector exposes a read/write interface.
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * // mutable vectors get a full-featured "manager":
     * geo::Point_t p { 1.0, 2.0, 3.0 };
     * auto px
     *   = geo::vect::bindCoord(p, geo::vect::coordManager<geo::Point_t>(0U));
     * px *= 5.0;
     * std::cout << p << " has now x=" << px() << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will print something like:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * (5,2,3) has now x=5
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *
     * For a constant vector, the returned manager exposes a read-only
     * interface.
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * // constant vectors get a "reader" (read-only manager):
     * geo::Vector_t v { 1.0, 2.0, 3.0 };
     *
     * auto vx = geo::vect::bindCoord
     *   (v, geo::vect::coordManager<geo::Vector_t const>(1U));
     * std::cout << v << " has y=" << vy() << std::endl;
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * will print something like:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * {1,2,3) has y=2
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Note that the use in these examples, `coord()` is preferred.
     */
    template <typename Vector>
    constexpr auto coordManager(unsigned int n);

    /**
     * @brief Returns an object that can be bound to a vector to manage one of
     *        its coordinates.
     * @tparam Vector type of vector to get a manager for (constantness matters)
     * @param n index of the coordinate (`0`: X, `1`: Y, `2`: Z, `3`: T)
     * @param v a vector of type `Vector` (ignored)
     * @return a coordinate manager, undefined if index is invalid
     * @see `geo::vect::coordManager(unsigned int)`
     *
     * An alias of `geo::vect::coordManager(unsigned int)`.
     */
    template <typename Vector>
    constexpr auto coordManager(unsigned int n, Vector& v);

    //@{
    /// Returns an array with all coordinate managers for a type of vector.
    template <typename Vector>
    constexpr auto coordManagers();
    template <typename Vector>
    constexpr auto coordManagers(Vector&&);
    //@}

    //@{
    /// Returns an array with all coordinate readers for a type of vector.
    template <typename Vector>
    constexpr auto coordReaders();
    template <typename Vector>
    constexpr auto coordReaders(Vector&&);
    //@}

    /// Binds the specified constant vector to the coordinate reader.
    template <typename Vector>
    constexpr auto bindCoord(Vector const& v, CoordReader_t<Vector> helper)
    {
      return details::BoundCoordGetter<CoordReader_t<Vector>, Vector const>(v, helper);
    }

    /// Binds the specified vector to the coordinate manager.
    template <typename Vector>
    auto bindCoord(Vector& v, CoordManager_t<Vector> helper)
      -> details::BoundCoordManager<CoordManager_t<Vector>, Vector>
    {
      return {v, helper};
    }

    /**
     * @brief Returns an object to manage the coordinate X of the vector `v`.
     * @tparam Vector the type of vector to be managed
     * @param v the vector with the coordinate X to be managed
     * @return an object to manage the coordinate X of `v`
     *
     * The type `Vector` needs to have a `double X() const` method and a
     * `auto SetX(auto)` method, where the argument is the type of the managed
     * coordinate and the return value is unspecified.
     */
    template <typename Vector>
    auto Xcoord(Vector& v)
    {
      return bindCoord(v, XcoordManager<Vector>);
    }

    /**
     * @brief Returns an object to manage the coordinate Y of the vector `v`.
     * @tparam Vector the type of vector to be managed
     * @param v the vector with the coordinate Y to be managed
     * @return an object to manage the coordinate Y of `v`
     *
     * The type `Vector` needs to have a `double Y() const` method and a
     * `auto SetY(auto)` method, where the argument is the type of the managed
     * coordinate and the return value is unspecified.
     */
    template <typename Vector>
    auto Ycoord(Vector& v)
    {
      return bindCoord<Vector>(v, YcoordManager<Vector>);
    }

    /**
     * @brief Returns an object to manage the coordinate Z of the vector `v`.
     * @tparam Vector the type of vector to be managed
     * @param v the vector with the coordinate Z to be managed
     * @return an object to manage the coordinate Z of `v`
     *
     * The type `Vector` needs to have a `double Z() const` method and a
     * `auto SetZ(auto)` method, where the argument is the type of the managed
     * coordinate and the return value is unspecified.
     */
    template <typename Vector>
    auto Zcoord(Vector& v)
    {
      return bindCoord<Vector>(v, ZcoordManager<Vector>);
    }

    /**
     * @brief Returns an object to manage the coordinate T of the vector `v`.
     * @tparam Vector the type of vector to be managed
     * @param v the vector with the coordinate T to be managed
     * @return an object to manage the coordinate T of `v`
     *
     * The type `Vector` needs to have a `double T() const` method and a
     * `auto SetT(auto)` method, where the argument is the type of the managed
     * coordinate and the return value is unspecified.
     */
    template <typename Vector>
    auto Tcoord(Vector& v)
    {
      return bindCoord<Vector>(v, TcoordManager<Vector>);
    }

    /**
     * @brief Returns an object to manage the coordinate `n` of a vector
     * @tparam Vector the type of vector to be managed
     * @param v the vector to be managed
     * @param n the coordinate index: `0` for X, `1` for Y and `2` for Z
     * @return an object to manage the coordinate `n` of a vector
     * @see `Xcoord()`, `Ycoord()`, `Zcoord()`
     *
     * Result is undefined for any value of `n` other than `0`, `1` and `2`.
     * See `Xcoord()`, `Ycoord()` and `Zcoord()` for `Vector` type requirements.
     */
    template <typename Vector>
    auto coord(Vector& v, unsigned int n) noexcept;

    template <typename Vector>
    auto coord(Vector& v, Coordinate n) noexcept;

    /// Returns an array with all coordinate managers bound to the specified
    /// vector.
    template <typename Vector>
    constexpr auto bindCoordManagers(Vector& v);

    /// Returns an array with all coordinate readers bound to the specified
    /// vector.
    template <typename Vector>
    constexpr auto bindCoordReaders(Vector const& v);

    /**
     * @brief Constant iterator to vector coordinates.
     * @tparam Vector the type of vector being iterated
     * @see `vector_cbegin()`, `vector_cend()`, `iterateCoords()`
     * @todo Not fully unit-tested yet!
     */
    template <typename Vector>
    class CoordConstIterator {

    public:
      using iterator_t = CoordConstIterator<Vector>; ///< Type of this iterator.

    private:
      using Vector_t = Vector; ///< Type of vector being iterated.
      /// Type of vector coordinate.
      using Coord_t = vect::coordinate_t<Vector_t>;

      Vector* v = nullptr;
      unsigned int index = std::numeric_limits<unsigned int>::max();

      decltype(auto) access(unsigned int c) const { return vect::bindCoord(v, index); }

      iterator_t copy() const { return *this; }

    public:
      /// --- BEGIN Iterator traits ------------------------------------------------
      /// @name Iterator traits
      /// @{
      using value_type = std::remove_cv_t<Coord_t>;
      using difference_type = std::ptrdiff_t;
      using reference = Coord_t;
      using pointer = Coord_t const*;
      using iterator_category = std::random_access_iterator_tag;
      /// @}
      /// --- END Iterator traits --------------------------------------------------

      /// Default constructor: invalid iterator.
      CoordConstIterator() = default;

      /// Constructor: points to `index` coordinate of vector `v`.
      CoordConstIterator(Vector& v, unsigned int index = 0) : v(&v), index(index) {}

      // --- BEGIN Access ----------------------------------------------------------
      /// @name Access
      /// @{

      /// Access the current coordinate.
      reference operator*() const { return vect::coord(*v, index); }

      /// Access the current coordinate.
      reference operator[](difference_type n) const { return vect::coord(*v, index + n); }

      /// @}
      // --- END Access ------------------------------------------------------------

      // --- BEGIN Moving ----------------------------------------------------------
      /// @name Moving
      /// @{

      /// Points to the next coordinate (unchecked).
      iterator_t& operator++()
      {
        ++index;
        return *this;
      }

      /// Points to the next coordinate (unchecked), returns the previous iterator.
      iterator_t operator++(int)
      {
        iterator_t it(*this);
        ++index;
        return it;
      }

      /// Points to the previous coordinate (unchecked).
      iterator_t& operator--()
      {
        --index;
        return *this;
      }

      /// Points to previous coordinate (unchecked), returns the previous iterator.
      iterator_t operator--(int)
      {
        iterator_t it(*this);
        --index;
        return it;
      }

      /// Increments the iterator by `d` positions (unchecked).
      iterator_t& operator+=(difference_type d)
      {
        index += d;
        return *this;
      }

      /// Returns an iterator incremented by `d` positions (unchecked).
      iterator_t operator+(difference_type d) const { return (copy() += d); }

      /// Decrements the iterator by `d` positions (unchecked).
      iterator_t& operator-=(difference_type d)
      {
        index -= d;
        return *this;
      }

      /// Returns an iterator decremented by `d` positions (unchecked).
      iterator_t operator-(difference_type d) const { return (copy() -= d); }

      /// Returns the distance from another iterator.
      iterator_t operator-(iterator_t const& other) const { return index - other.index; }

      ///@}
      // --- END Moving ------------------------------------------------------------

      // --- BEGIN Comparison operators --------------------------------------------
      /// @name Comparison operators
      ///
      /// All comparisons between iterators on different vectors fail.
      /// @{

      bool operator==(iterator_t const& other) const
      {
        return (v == other.v) && (index == other.index);
      }

      bool operator!=(iterator_t const& other) const
      {
        return (v != other.v) || (index != other.index);
      }

      bool operator<=(iterator_t const& other) const
      {
        return (v == other.v) && (index <= other.index);
      }

      bool operator>=(iterator_t const& other) const
      {
        return (v == other.v) && (index >= other.index);
      }

      bool operator<(iterator_t const& other) const
      {
        return (v == other.v) && (index < other.index);
      }

      bool operator>(iterator_t const& other) const
      {
        return (v == other.v) && (index > other.index);
      }

      /// @}
      // --- END Comparison operators ----------------------------------------------

    }; // class CoordConstIterator

    template <typename Vector>
    CoordConstIterator<Vector> operator+(typename CoordConstIterator<Vector>::difference_type n,
                                         CoordConstIterator<Vector> const& v)
    {
      return v + n;
    }

    /// Returns a const-iterator pointing to the first coordinate of `v`.
    template <typename Vector>
    auto vector_cbegin(Vector const& v)
    {
      return CoordConstIterator(v, 0);
    }

    /// Returns a const-iterator pointing after the last coordinate of `v`.
    template <typename Vector>
    auto vector_cend(Vector const& v)
    {
      return CoordConstIterator(v, vect::dimension(v));
    }

    /**
     * @brief Returns a vector of type `Dest` with the same content as a `Src`.
     * @tparam Dest target vector type
     * @tparam Source type of the vector to be converted from
     * @param v the vector to be converted from
     * @return a vector with the same content as `v`, but of type `Dest`
     *
     * For this to work, both `Src` and `Dest` types must satisfy the
     * requirements of `Xcoord()`, `Ycoord()`, `Zcoord()` etc.
     */
    template <typename Dest, typename Source>
    Dest convertTo(Source const& v);

    /**
     * @brief Returns a vector of type `Dest` with the same content as a `Src`.
     * @tparam Dest target vector type
     * @tparam Source type of the vector to be converted from
     * @param coll the collection of vectors to be converted from
     * @return a collection of vectors with the same content as `coll`, but of
     *         type `Dest`
     * @see `convertTo()`
     *
     * This version applies `convertTo()` to all the elements of the specified
     * collection, returning a collection of the same template type
     * (`std::vector`).
     *
     * For the requirements, see `convertTo()`.
     */
    template <typename Dest, typename Source>
    std::vector<Dest> convertCollTo(std::vector<Source> const& coll);

    /**
     * @brief Returns a new vector applying a predicate to each component.
     * @tparam Vector type of the vector in input (and output)
     * @tparam Pred type of unary predicate to be applied
     * @param v the vector to be transformed
     * @param pred the predicate to be applied
     * @return a vector equivelent to `{ pred(x), pred(y), ... }`
     *
     * The predicate is a "functor" type, taking as the single argument the
     * coordinate to be transformed, and returning the transformed value of it.
     */
    template <typename Vector, typename Pred>
    Vector transformCoords(Vector const& v, Pred&& pred);

    /// @}
    // --- END Vector coordinate access abstraction ----------------------------

    // --- BEGIN Functions for common vector operations ------------------------
    /// @{
    /** ************************************************************************
     * @name Functions for common vector operations.
     *
     * This group of template functions are meant to be used with vectors in a
     * generic way.
     * The default implementation is for `TVector3`. Specializations can be
     * easily written for other vector types.
     *
     * In addition, two "standard" representations for vectors and points are
     * provided.
     *
     * @note The representations for vector and point objects are currently the
     *       same; this prevents relying on overload resolution to decide which
     *       function to use. For example, defining two functions with signature:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * Vector_t Project(Vector_t const& v);
     * Point_t Project(Point_t const& v);
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *       will not compile since these two are exactly the same.
     *       A solution might be to derive two different classes from the common
     *       one:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * struct Vector_t: public VectorBase_t { using VectorBase_t::VectorBase_t; };
     * struct Point_t: public VectorBase_t { using VectorBase_t::VectorBase_t; };
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *       This will likely have consequences though (for example, the sum of
     *       two `Vector_t` or `Point_t` will become a `VectorBase_t`).
     *
     */

    /// Returns a vector with all components rounded if close to 0.
    template <typename Vector, typename Scalar>
    Vector rounded0(Vector const& v, Scalar tol)
    {
      return transformCoords(v, [tol](auto c) { return extra::roundValue0(c, tol); });
    }

    /// Returns a vector with all components rounded if close to 0.
    template <typename Vector, typename Scalar>
    void round0(Vector& v, Scalar tol)
    {
      v = rounded0(v, tol);
    }

    /// Returns a vector with all components rounded if close to 0, -1 or +1.
    template <typename Vector, typename Scalar>
    Vector rounded01(Vector const& v, Scalar tol)
    {
      return transformCoords(v, [tol](auto c) { return extra::roundValue01(c, tol); });
    }

    /// Returns a vector with all components rounded if close to 0, -1 or +1.
    template <typename Vector, typename Scalar>
    void round01(Vector& v, Scalar tol)
    {
      v = rounded01(v, tol);
    }

    /// Returns whether all components of the vector are finite.
    template <typename Vector>
    bool isfinite(Vector const& v);

    /// Returns a vector parallel to v and with norm 1
    template <typename Vector>
    Vector normalize(Vector const& v)
    {
      return v.Unit();
    }

    /// Return cross product of two vectors
    template <typename Vector>
    Vector cross(Vector const& a, Vector const& b)
    {
      return a.Cross(b);
    }

    /// Return cross product of two vectors
    template <typename Vector, typename OtherVector>
    constexpr auto dot(Vector const& a, OtherVector const& b)
    {
      return a.Dot(b);
    }

    /// Return norm of the specified vector
    template <typename Vector>
    auto mag2(Vector const& v)
    {
      return v.Mag2();
    }

    /// Return norm of the specified vector
    template <typename Vector>
    auto norm(Vector const& v)
    {
      return v.Mag();
    }

    /// Return "mixed" product of three vectors:
    /// @f$ \vec{a} \times \vec{b} \cdot \vec{c} @f$
    template <typename Vector>
    auto mixedProduct(Vector const& a, Vector const& b, Vector const& c)
    {
      return dot(cross(a, b), c);
    }

    /// @}
    // --- END Functions for common vector operations --------------------------

    /** ************************************************************************
     * @brief Helper class to compute the middle point in a point set.
     * @tparam N _(default: `3`)_ dimension of the points
     *
     * This class accumulates cartesian points and returns their middle point
     * when asked.
     *
     * In the following example, only the points from a list (`points`) which
     * have _y_ coordinate larger than 0 are averaged, all with the same weight:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * std::array<geo::Point_t, 4> const points = {
     *   geo::Point_t{ 0.0,  1.0,  2.0 },
     *   geo::Point_t{ 0.0, -1.0,  2.0 },
     *   geo::Point_t{ 0.0,  1.0, -2.0 },
     *   geo::Point_t{ 0.0, -1.0, -2.0 }
     * };
     *
     * geo::vect::MiddlePointAccumulatorDim pointsAboveGround;
     * for (auto const& point: points)
     *   if (point.Y() > 0.0) pointsAboveGround.add(point);
     *
     * if (pointsAboveGround.empty())
     *   throw std::runtime_error("No point above ground!");
     *
     * auto middleAboveGround = pointsAboveGround.middlePoint();
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Note the check to make sure that there are points that fulfil the
     * requirement.
     *
     */
    template <unsigned int N = 3U>
    class MiddlePointAccumulatorDim {
      static constexpr unsigned int Dim = N; ///< Dimension of the points.
      std::array<Length_t, Dim> fSums;       ///< Sum of each of the point components.
      double fW = 0.0;                       ///< Total weight.

    public:
      /// Default constructor: starts with no accumulated point.
      MiddlePointAccumulatorDim() { fSums.fill(0.); }

      /**
       * @brief Constructor: starts with accumulating a sequence of points.
       * @tparam BeginIter type of iterator to a point type compatible with add()
       * @tparam EndIter type of end iterator
       * @param begin iterator to the first point to be added
       * @param end iterator after the last point to be added
       * @see add()
       */
      template <typename BeginIter, typename EndIter>
      MiddlePointAccumulatorDim(BeginIter begin, EndIter end) : MiddlePointAccumulatorDim()
      {
        add(begin, end);
      }

      // --- BEGIN Result query ------------------------------------------------
      /// @{
      /// @name Result query

      /// Returns whether the total weight is zero (usually means no points).
      bool empty() const { return fW == 0.0; }

      /// Returns the total weight (number of points if all have weight 1).
      double weight() const { return fW; }

      /**
       * @brief Returns the middle point, NaN components if no point.
       * @tparam Point type of the output point
       *
       * The type of return point must be specified as template argument, e.g.
       * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
       * auto mp = accumulator.middlePointAs<TVector3>();
       * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       * The `Point` type is required to have a constructor with the three
       * cartesian components as arguments.
       */
      template <typename Point>
      Point middlePointAs() const
      //  { return { fSums[0] / fW, fSums[1] / fW, fSums[2] / fW }; }
      {
        return makeWeightedPoint<Point>(1.0 / fW);
      }

      /// Returns the middle point as a `geo::Point_t`,
      /// NaN components if no point.
      Point_t middlePoint() const { return middlePointAs<Point_t>(); }

      /// @}
      // --- END Result query --------------------------------------------------

      // --- BEGIN Addition of points ------------------------------------------
      /// @{
      /// @name Addition of points

      /**
       * @brief Accumulates a point.
       * @tparam Point point type, required to have X(), Y() and Z() accessors
       * @param p point to be included
       *
       * The point is added with weight 1.
       */
      template <typename Point>
      void add(Point const& p)
      {
        std::size_t ic = 0U;
        for (auto c : vect::bindCoordManagers(p))
          fSums[ic++] += c();
        fW += 1.0;
      }

      /**
       * @brief Accumulates a point.
       * @tparam Point point type, required to have X(), Y() and Z() accessors
       * @param p point to be included
       * @param weight the relative weight of this point
       */
      template <typename Point>
      void add(Point const& p, double weight)
      {
        std::size_t ic = 0U;
        for (auto c : vect::bindCoordManagers(p))
          fSums[ic++] += weight * c();
        fW += weight;
      }

      /**
       * @brief Adds a sequence of points.
       * @tparam BeginIter type of iterator to a point type compatible with add()
       * @tparam EndIter type of end iterator
       * @param begin iterator to the first point to be added
       * @param end iterator after the last point to be added
       *
       * Each point is added with weight 1.0.
       */
      template <typename BeginIter, typename EndIter>
      void add(BeginIter begin, EndIter end)
      {
        std::for_each(begin, end, [this](auto const& p) { this->add(p); });
      }

      /// Resets the status of the object to no accumulated points.
      void clear()
      {
        fSums.fill(0.);
        fW = 0.0;
      }

      /// @}
      // --- END Addition of points --------------------------------------------

    private:
      using IndexSequence_t = std::make_index_sequence<Dim>;

      template <typename Point, std::size_t... I>
      Point makePointImpl(std::index_sequence<I...>) const
      {
        return {fSums.operator[](I)...};
      }

      template <typename Point, std::size_t... I>
      Point makeWeightedPointImpl(double w, std::index_sequence<I...>) const
      {
        return {(fSums.operator[](I) * w)...};
      }

      /// Converts the internal sums into a `Point`.
      template <typename Point>
      Point makePoint() const
      {
        return vect::makeFromCoords<Point>(fSums);
      }

      /// Converts the internal sums into a `Point` with components scaled by
      /// `w`.
      template <typename Point>
      Point makeWeightedPoint(double w) const
      {
        return makeWeightedPointImpl<Point>(w, IndexSequence_t{});
      }

    }; // MiddlePointAccumulatorDim()

    /// Middle-point accumulator for vectors of dimension 3.
    using MiddlePointAccumulator = MiddlePointAccumulatorDim<3U>;

    // --- BEGIN Middle point functions ----------------------------------------
    /// @{
    /// @name Middle point functions

    /**
     * @brief Returns the middle of the specified points.
     * @tparam Point cartesian-represented point type with 3-component constructor
     * @tparam BeginIter type of iterator to a point type compatible with add()
     * @tparam EndIter type of end iterator
     * @param begin iterator to the first point to be averaged
     * @param end iterator after the last point to be averaged
     * @return an object of type `Point` with the value of the middle point
     *
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * std::vector<geo::Point_t> points {
     *   geo::Point_t(1., 2., 3.),
     *   geo::Point_t(2., 4., 6.),
     *   geo::Point_t(3., 6., 9.)
     *   };
     *
     * auto mp = geo::vect::middlePointAs<geo::Vector_t>(points.begin(), points.end());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * The variable `mp` of the example will be of type `geo::Vector_t` (converted
     * component by components from a `geo::Point_t`).
     *
     */
    template <typename Point, typename BeginIter, typename EndIter>
    Point middlePointAs(BeginIter begin, EndIter end)
    {
      constexpr auto Dim = vect::dimension<Point>();
      return MiddlePointAccumulatorDim<Dim>(begin, end).template middlePointAs<Point>();
    }

    /**
     * @brief Returns the middle of the specified points.
     * @tparam BeginIter type of iterator to a point type compatible with add()
     * @tparam EndIter type of end iterator
     * @param begin iterator to the first point to be averaged
     * @param end iterator after the last point to be averaged
     * @return an object of type `Point_t` with the value of the middle point
     *
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * std::vector<geo::Point_t> points {
     *   geo::Point_t(1., 2., 3.),
     *   geo::Point_t(2., 4., 6.),
     *   geo::Point_t(3., 6., 9.)
     *   };
     *
     * auto mp = geo::vect::middlePoint(points.begin(), points.end());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * The variable `mp` of the example will be of type `geo::Point_t`.
     *
     */
    template <typename BeginIter, typename EndIter>
    Point_t middlePoint(BeginIter begin, EndIter end)
    {
      return middlePointAs<Point_t>(begin, end);
    }

    /**
     * @brief Returns the middle of the specified points.
     * @tparam Point cartesian-represented point type with 3-component constructor
     *         and X(), Y() and Z() accessors.
     * @param points the list of points to be included
     * @return the value of the middle point
     *
     * Example of usage:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
     * auto mp = geo::vect::middlePoint
     *   ({ geo::Point_t(1., 2., 3.), geo::Point_t(3., 6., 9.) });
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * The variable `mp` will contain the middle point between the two specified
     * in the initializer list.
     *
     */
    template <typename Point>
    Point middlePoint(std::initializer_list<Point> points)
    {
      constexpr auto Dim = vect::dimension<Point>();
      return MiddlePointAccumulatorDim<Dim>(points.begin(), points.end())
        .template middlePointAs<Point>();
    }

    /// @}
    // --- END Middle point functions ------------------------------------------

    // --- BEGIN Support for LArSoft geometry vectors --------------------------
    /// @{
    /// @name Support for LArSoft geometry vectors

    /// Convert the specified point into a `geo::Point_t`.
    template <typename Point>
    Point_t toPoint(Point const& p)
    {
      return vect::convertTo<Point_t>(p);
    }

    /// Convert the specified vector into a `geo::Vector_t`.
    template <typename Vector>
    Vector_t toVector(Vector const& v)
    {
      return vect::convertTo<Vector_t>(v);
    }

    /// Convert the specified collection of points into a collection of
    /// `geo::Point_t`.
    template <typename Point>
    std::vector<Point_t> convertCollToPoint(std::vector<Point> const& coll)
    {
      return convertCollTo<Point_t>(coll);
    }

    /// Convert the specified collection of vectors into a collection of
    /// `geo::Vector_t`.
    template <typename Vector>
    std::vector<Vector_t> convertCollToVector(std::vector<Vector> const& coll)
    {
      return convertCollTo<Vector_t>(coll);
    }

    /// Creates a `geo::Point_t` from its coordinates (see `makeFromCoords()`).
    template <typename Coords>
    GENVECTOR_CONSTEXPR Point_t makePointFromCoords(Coords&& coords)
    {
      return makeFromCoords<Point_t>(std::forward<Coords>(coords));
    }

    /// Creates a `geo::Vector_t` from its coordinates (see `makeFromCoords()`).
    template <typename Coords>
    GENVECTOR_CONSTEXPR Vector_t makeVectorFromCoords(Coords&& coords)
    {
      return makeFromCoords<Vector_t>(std::forward<Coords>(coords));
    }

    /// @}
    // --- END Support for LArSoft geometry vectors ----------------------------

    /// @}
    // END Geometry group ------------------------------------------------------

  } // namespace vect

} // namespace geo

//------------------------------------------------------------------------------
//---  STL specialization for ROOT GenVector vectors
//------------------------------------------------------------------------------
namespace ROOT::Math {

  /// @name Overloads of STL C++ functions for ROOT GenVector vectors
  /// @{

  // --- BEGIN 2D vectors ------------------------------------------------------
  template <class CoordSystem, class Tag>
  decltype(auto) begin(ROOT::Math::PositionVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cbegin(ROOT::Math::PositionVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) end(ROOT::Math::PositionVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cend(ROOT::Math::PositionVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) begin(ROOT::Math::DisplacementVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cbegin(ROOT::Math::DisplacementVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) end(ROOT::Math::DisplacementVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cend(ROOT::Math::DisplacementVector2D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  // --- END 2D vectors --------------------------------------------------------

  // --- BEGIN 3D vectors ------------------------------------------------------
  template <class CoordSystem, class Tag>
  decltype(auto) begin(ROOT::Math::PositionVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cbegin(ROOT::Math::PositionVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) end(ROOT::Math::PositionVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cend(ROOT::Math::PositionVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) begin(ROOT::Math::DisplacementVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cbegin(ROOT::Math::DisplacementVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) end(ROOT::Math::DisplacementVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem, class Tag>
  decltype(auto) cend(ROOT::Math::DisplacementVector3D<CoordSystem, Tag> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  // --- END 3D vectors --------------------------------------------------------

  // --- BEGIN 4D vectors ------------------------------------------------------
  template <class CoordSystem>
  decltype(auto) begin(ROOT::Math::LorentzVector<CoordSystem> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem>
  decltype(auto) cbegin(ROOT::Math::LorentzVector<CoordSystem> const& v)
  {
    return geo::vect::vector_cbegin(v);
  }

  template <class CoordSystem>
  decltype(auto) end(ROOT::Math::LorentzVector<CoordSystem> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  template <class CoordSystem>
  decltype(auto) cend(ROOT::Math::LorentzVector<CoordSystem> const& v)
  {
    return geo::vect::vector_cend(v);
  }

  // --- END 4D vectors --------------------------------------------------------

  /// @}

} // namespace ROOT::Math

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//---  template specializations for standard geometry vectors
//---
namespace geo::vect {

  //----------------------------------------------------------------------------
  template <>
  inline auto norm(geo::Vector_t const& v)
  {
    return v.R();
  }

  //----------------------------------------------------------------------------

} // namespace geo::vect

//------------------------------------------------------------------------------
//---  template implementation

namespace geo::vect::details {

  //----------------------------------------------------------------------------
  template <typename>
  struct AlwaysFalse : std::false_type {};

  // constexpr variant of forward (from StackExchange)
  template <typename T>
  constexpr T&& constexpr_forward(std::remove_reference_t<T>& t)
  {
    return static_cast<T&&>(t);
  }

  template <typename T>
  constexpr T&& constexpr_forward(std::remove_reference_t<T>&& t)
  {
    static_assert(!std::is_lvalue_reference<T>(),
                  "template argument substituting T is an lvalue reference type");
    return static_cast<T&&>(t);
  }

  //----------------------------------------------------------------------------
  template <typename Vector, typename /* = void */>
  struct DimensionImpl : public std::integral_constant<unsigned int,
                                                       HasT<Vector>() ? 4U :
                                                       HasZ<Vector>() ? 3U :
                                                       HasY<Vector>() ? 2U :
                                                       HasX<Vector>() ? 1U :
                                                                        0U> {};

  template <typename Array>
  struct DimensionImpl<Array, std::enable_if_t<(std::extent_v < Array >> 0)>>
    : public std::integral_constant<unsigned int, std::extent_v<Array>> {};

  template <typename T, std::size_t Dim>
  struct DimensionImpl<std::array<T, Dim>, void>
    : public std::integral_constant<unsigned int, Dim> {};

  template <typename T>
  struct DimensionImpl<lar::util::simple_geo::Point2D<T>, void>
    : public std::integral_constant<unsigned int, 2> {};

  template <typename T>
  struct DimensionImpl<lar::util::simple_geo::Point3D<T>, void>
    : public std::integral_constant<unsigned int, 3> {};

  //----------------------------------------------------------------------------
  /// Type of sequence of indices up to `Vector` size.
  template <typename Vector>
  using VectorIndices_t = std::make_index_sequence<dimension<Vector>()>;

  template <typename Vector>
  constexpr auto makeVectorIndices()
  {
    return VectorIndices_t<Vector>{};
  }

  template <typename Vector>
  constexpr auto makeVectorIndices(Vector&&)
  {
    return makeVectorIndices<Vector>();
  }

  template <typename T, T... Indices>
  constexpr auto makeIndexSeqImpl(std::integer_sequence<T, Indices...>)
  // BUG the double brace syntax is required to work around clang bug 21629
  // (https://bugs.llvm.org/show_bug.cgi?id=21629)
  {
    return std::array<T, sizeof...(Indices)>{{Indices...}};
  }

  // fill a sequence object with the first `N` values of type `T`.
  template <typename T, T N>
  constexpr auto makeIndexSeq()
  {
    return makeIndexSeqImpl<T>(std::make_integer_sequence<T, N>{});
  }

  //----------------------------------------------------------------------------
  template <std::size_t I, typename Data>
  constexpr auto accessElement(Data&& data)
  {
    return data[I];
  }

  template <typename Vector, typename Coords, std::size_t... Indices>
  constexpr Vector makeFromCoordsImpl(Coords&& coords, std::index_sequence<Indices...>)
  {
    return {accessElement<Indices>(constexpr_forward<Coords>(coords))...};
  }

  //----------------------------------------------------------------------------
  template <typename Vector>
  constexpr CoordManager_t<Vector> NoCoordManager{nullptr, nullptr};

  template <typename Vector, unsigned int Dim = dimension<Vector>()>
  struct CoordManagerImpl;

  template <typename Vector>
  struct CoordManagerImpl<Vector, 1U> {
    static auto get(unsigned int n) noexcept
    {
      return (n == 0) ? XcoordManager<Vector> : NoCoordManager<Vector>;
    }
  };

  template <typename Vector>
  struct CoordManagerImpl<Vector, 2U> {
    static auto get(unsigned int n) noexcept
    {
      return (n == 1) ? YcoordManager<Vector> : CoordManagerImpl<Vector, 1U>::get(n);
    }
  };

  template <typename Vector>
  struct CoordManagerImpl<Vector, 3U> {
    static auto get(unsigned int n) noexcept
    {
      return (n == 2) ? ZcoordManager<Vector> : CoordManagerImpl<Vector, 2U>::get(n);
    }
  };

  template <typename Vector>
  struct CoordManagerImpl<Vector, 4U> {
    static auto get(unsigned int n) noexcept
    {
      return (n == 3) ? TcoordManager<Vector> : CoordManagerImpl<Vector, 3U>::get(n);
    }
  };

  //----------------------------------------------------------------------------
  template <typename Vector, unsigned int N>
  struct CoordManagersImplBase {
    static constexpr unsigned int Dim = N;

    using Manager_t = decltype(XcoordManager<Vector>);
    using Return_t = std::array<Manager_t, Dim>;

    static_assert(dimension<Vector>() == Dim, "Inconsistent vector size.");
  };

  template <typename Vector, unsigned int N>
  struct CoordManagersImpl;

  template <typename Vector>
  struct CoordManagersImpl<Vector, 2U> : private CoordManagersImplBase<Vector, 2U> {
    using Base_t = CoordManagersImplBase<Vector, 2U>;
    using typename Base_t::Return_t;
    static constexpr Return_t get()
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{XcoordManager<Vector>, YcoordManager<Vector>}};
    }
  };

  template <typename Vector>
  struct CoordManagersImpl<Vector, 3U> : private CoordManagersImplBase<Vector, 3U> {
    using Base_t = CoordManagersImplBase<Vector, 3U>;
    using typename Base_t::Return_t;
    static constexpr Return_t get()
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{XcoordManager<Vector>, YcoordManager<Vector>, ZcoordManager<Vector>}};
    }
  };

  template <typename Vector>
  struct CoordManagersImpl<Vector, 4U> : private CoordManagersImplBase<Vector, 4U> {
    using Base_t = CoordManagersImplBase<Vector, 4U>;
    using typename Base_t::Return_t;
    static constexpr Return_t get()
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{XcoordManager<Vector>,
               YcoordManager<Vector>,
               ZcoordManager<Vector>,
               TcoordManager<Vector>}};
    }
  };

  //----------------------------------------------------------------------------
  template <typename Vector, unsigned int N>
  struct BindCoordManagersImplBase {
    static constexpr unsigned int Dim = N;

    using Manager_t = decltype(Xcoord(std::declval<Vector>()));
    using Return_t = std::array<Manager_t, Dim>;

    static_assert(dimension<Vector>() == Dim, "Inconsistent vector size.");
  };

  template <typename Vector, unsigned int N>
  struct BindCoordManagersImpl;

  template <typename Vector>
  struct BindCoordManagersImpl<Vector, 2U> : private BindCoordManagersImplBase<Vector, 2U> {
    using Base_t = CoordManagersImplBase<Vector, 2U>;
    using typename Base_t::Return_t;
    static Return_t bind(Vector& v)
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{Xcoord(v), Ycoord(v)}};
    }
  };

  template <typename Vector>
  struct BindCoordManagersImpl<Vector, 3U> : private BindCoordManagersImplBase<Vector, 3U> {
    using Base_t = BindCoordManagersImplBase<Vector, 3U>;
    using typename Base_t::Return_t;
    static Return_t bind(Vector& v)
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{Xcoord(v), Ycoord(v), Zcoord(v)}};
    }
  };

  template <typename Vector>
  struct BindCoordManagersImpl<Vector, 4U> : private BindCoordManagersImplBase<Vector, 4U> {
    using Base_t = BindCoordManagersImplBase<Vector, 4U>;
    using typename Base_t::Return_t;
    static Return_t bind(Vector& v)
    {
      // BUG the double brace syntax is required to work around clang bug 21629
      // (https://bugs.llvm.org/show_bug.cgi?id=21629)
      return {{Xcoord(v), Ycoord(v), Zcoord(v), Tcoord(v)}};
    }
  };

  //----------------------------------------------------------------------------
  template <typename Dest, typename Source>
  struct ConvertToImplBase {
    static_assert(dimension<Source>() == dimension<Dest>(),
                  "Source and destination vectors must have the same dimension.");
  };

  template <typename Dest, typename Source, unsigned int Dim>
  struct ConvertArrayTo : private ConvertToImplBase<Dest, Source> {
    static_assert(std::is_arithmetic_v<std::decay_t<decltype(std::declval<Source>()[0])>>);
    static Dest convert(Source const& v) { return geo::vect::makeFromCoords<Dest>(v); }
  };

  // will handle cases with specific dimensionality
  template <typename Dest, typename Source, unsigned int Dim>
  struct ConvertToImplDim;

  template <typename Dest, typename Source>
  struct ConvertToImpl : public ConvertToImplDim<Dest, Source, dimension<Source>()> {};

  template <typename Dest, typename T, std::size_t Dim>
  struct ConvertToImpl<Dest, std::array<T, Dim>>
    : public ConvertArrayTo<Dest, std::array<T, Dim>, Dim> {};

  template <typename Dest, typename T, std::size_t Dim>
  struct ConvertToImpl<Dest, T[Dim]> : public ConvertArrayTo<Dest, T[Dim], Dim> {};

  // special special implementation for T*, since we can't guess its dimension
  template <typename Dest, typename T>
  struct ConvertToImpl<Dest, T*> {
    static_assert(std::is_arithmetic_v<T>);
    static Dest convert(T* v) { return geo::vect::makeFromCoords<Dest>(v); }
  }; // struct ConvertToImpl<T*>

  // handled cases with specific dimensionality
  template <typename Dest, typename Source, unsigned int Dim>
  struct ConvertToImplDim {
    // trivial to do: open a feature request!
    static_assert(AlwaysFalse<Dest>(), "This vector dimensionality is not implemented yet.");
  };

  template <typename Dest, typename Source>
  struct ConvertToImplDim<Dest, Source, 2U> : private ConvertToImplBase<Dest, Source> {
    static Dest convert(Source const& v) { return {Xcoord(v)(), Ycoord(v)()}; }
  };

  template <typename Dest, typename Source>
  struct ConvertToImplDim<Dest, Source, 3U> : private ConvertToImplBase<Dest, Source> {
    static Dest convert(Source const& v) { return {Xcoord(v)(), Ycoord(v)(), Zcoord(v)()}; }
  };

  template <typename Dest, typename Source>
  struct ConvertToImplDim<Dest, Source, 4U> : private ConvertToImplBase<Dest, Source> {
    static Dest convert(Source const& v)
    {
      return {Xcoord(v)(), Ycoord(v)(), Zcoord(v)(), Tcoord(v)()};
    }
  };

  template <typename Dest, typename Source>
  struct ConvertToDispatcher : public ConvertToImpl<Dest, Source> {};

  // special pass-through case
  template <typename Vector>
  struct ConvertToDispatcher<Vector, Vector> {
    static constexpr decltype(auto) convert(Vector const& v) { return v; }
  };

  //----------------------------------------------------------------------------
  template <typename Point, std::size_t... I>
  bool isfiniteImpl(Point const& point, std::index_sequence<I...>)
  {
    return extended_and(std::isfinite(coord(point, I).get())...);
  }

  //----------------------------------------------------------------------------

} // namespace geo::vect::details

//------------------------------------------------------------------------------
template <typename Vector>
constexpr std::array<std::size_t, geo::vect::dimension<Vector>()> geo::vect::indices()
{
  return details::makeIndexSeq<std::size_t, dimension<Vector>()>();
}

template <typename Vector>
constexpr auto geo::vect::indices(Vector const&) -> decltype(indices<Vector>())
{
  return indices<Vector>();
}

//------------------------------------------------------------------------
template <typename Vector, typename Coords>
constexpr Vector geo::vect::makeFromCoords(Coords&& coords)
{
  using namespace geo::vect::details;
  return makeFromCoordsImpl<Vector>(constexpr_forward<Coords>(coords), makeVectorIndices<Vector>());
}

//------------------------------------------------------------------------------
template <typename Vector>
constexpr auto geo::vect::coordManager(unsigned int n)
{
  return details::CoordManagerImpl<Vector>::get(n);
}

//------------------------------------------------------------------------------
template <typename Vector>
auto geo::vect::coord(Vector& v, unsigned int n) noexcept
{
  return vect::bindCoord<Vector>(v, coordManager<Vector>(n));
}

//------------------------------------------------------------------------------
template <typename Vector>
auto geo::vect::coord(Vector& v, Coordinate coord) noexcept
{
  return coord(v, to_int(coord));
}

//------------------------------------------------------------------------
template <typename Vector, typename Coords>
unsigned int geo::vect::fillCoords(Coords& dest, Vector const& src)
{
  // this is simpler than makeFromCoords() because doesn't attempt constexpr
  for (std::size_t i = 0; i < vect::dimension(src); ++i)
    dest[i] = vect::coord(src, i);
  return vect::dimension(src);
}

//------------------------------------------------------------------------------
template <typename Vector>
constexpr auto geo::vect::coordManagers()
{
  using PlainVector = std::remove_reference_t<Vector>;
  return details::CoordManagersImpl<PlainVector, dimension<PlainVector>()>::get();
}

template <typename Vector>
constexpr auto geo::vect::coordManagers(Vector&&)
{
  return coordManagers<Vector>();
}

template <typename Vector>
constexpr auto geo::vect::coordReaders()
{
  using ConstVector = std::add_const_t<std::remove_reference_t<Vector>>;
  return details::CoordManagersImpl<ConstVector, dimension<ConstVector>()>::get();
}

template <typename Vector>
constexpr auto geo::vect::coordReaders(Vector&&)
{
  return coordReaders<Vector>();
}

//------------------------------------------------------------------------------
template <typename Vector>
constexpr auto geo::vect::bindCoordManagers(Vector& v)
{
  return details::BindCoordManagersImpl<Vector, dimension<Vector>()>::bind(v);
}

template <typename Vector>
constexpr auto geo::vect::bindCoordReaders(Vector const& v)
{
  using ConstVector = std::add_const_t<std::remove_reference_t<Vector>>;
  return details::BindCoordManagersImpl<ConstVector, dimension<ConstVector>()>::bind(v);
}

//------------------------------------------------------------------------------
template <typename Dest, typename Source>
Dest geo::vect::convertTo(Source const& v)
{
  return details::ConvertToDispatcher<Dest, Source>::convert(v);
}

//----------------------------------------------------------------------------
template <typename Dest, typename Source>
std::vector<Dest> geo::vect::convertCollTo(std::vector<Source> const& coll)
{
  std::vector<Dest> dest;
  dest.reserve(coll.size());
  std::transform(
    coll.begin(), coll.end(), std::back_inserter(dest), geo::vect::convertTo<Dest, Source>);
  return dest;
}

//------------------------------------------------------------------------
template <typename Vector, typename Pred>
Vector geo::vect::transformCoords(Vector const& v, Pred&& pred)
{
  details::CoordinateArray_t<Vector> values;
  unsigned int i = 0;
  for (auto c : bindCoordReaders(v))
    values[i++] = pred(c());
  return makeFromCoords<Vector>(values);
}

//------------------------------------------------------------------------------
template <typename Vector>
bool geo::vect::isfinite(Vector const& v)
{
  return details::isfiniteImpl(v, details::makeVectorIndices<Vector>());
}

//------------------------------------------------------------------------------

#endif // LARCOREALG_GEOMETRY_GEO_VECTORS_UTILS_H
