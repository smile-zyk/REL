// Value tests powered by GoogleTest.

#include "eval/value.h"

#include "data_array.h"
#include "data_series.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using rel::Value;

namespace
{
    // ---- helpers ------------------------------------------------------------

    xdataset::Measurement make_scalar_real(double v)
    {
        return xdataset::Measurement::Real(v);
    }

    xdataset::Measurement make_scalar_int(int v)
    {
        return xdataset::Measurement::Integer(v);
    }

    std::shared_ptr<xdataset::DataArray> make_data_array()
    {
        using namespace xdataset;
        return std::make_shared<DataArray>(
            DataArray::CreateIndependent(DataSeries::CreateScalar<double>()));
    }
} // namespace

// =========================================================================
//  Construction
// =========================================================================

TEST(ValueTest, DefaultConstructIsNull)
{
    Value v;
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_measurement());
    EXPECT_FALSE(v.is_data_array());
}

TEST(ValueTest, ConstructFromMeasurement)
{
    Value v(make_scalar_real(3.14));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_measurement());
    EXPECT_FALSE(v.is_data_array());
}

TEST(ValueTest, ConstructFromDataArray)
{
    auto da = make_data_array();
    Value v(da);
    EXPECT_FALSE(v.is_null());
    EXPECT_FALSE(v.is_measurement());
    EXPECT_TRUE(v.is_data_array());
}

// =========================================================================
//  Copy and move
// =========================================================================

TEST(ValueTest, CopyConstructor)
{
    Value v1(make_scalar_int(42));
    Value v2(v1);
    EXPECT_TRUE(v2.is_measurement());
    EXPECT_EQ(v2.as_measurement().as_scalar<int>(), 42);
}

TEST(ValueTest, CopyAssignment)
{
    Value v1(make_scalar_int(42));
    Value v2;
    v2 = v1;
    EXPECT_TRUE(v2.is_measurement());
}

TEST(ValueTest, MoveConstructor)
{
    Value v1(make_scalar_int(42));
    Value v2(std::move(v1));
    EXPECT_TRUE(v2.is_measurement());
    // After move, v1 is still valid (boost::variant move leaves source in
    // a valid but unspecified state; Measurement move is well-defined).
}

TEST(ValueTest, MoveAssignment)
{
    Value v1(make_scalar_int(42));
    Value v2;
    v2 = std::move(v1);
    EXPECT_TRUE(v2.is_measurement());
}

TEST(ValueTest, CopyDataArraySharesOwnership)
{
    auto da = make_data_array();
    Value v1(da);
    EXPECT_EQ(da.use_count(), 2); // da + v1
    {
        Value v2(v1);
        EXPECT_EQ(da.use_count(), 3); // da + v1 + v2
    }
    EXPECT_EQ(da.use_count(), 2); // v2 destroyed
}

// =========================================================================
//  Type queries
// =========================================================================

TEST(ValueTest, NullValueQueries)
{
    Value v;
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_measurement());
    EXPECT_FALSE(v.is_data_array());
}

TEST(ValueTest, MeasurementValueQueries)
{
    Value v(make_scalar_real(1.0));
    EXPECT_FALSE(v.is_null());
    EXPECT_TRUE(v.is_measurement());
    EXPECT_FALSE(v.is_data_array());
}

TEST(ValueTest, DataArrayValueQueries)
{
    Value v(make_data_array());
    EXPECT_FALSE(v.is_null());
    EXPECT_FALSE(v.is_measurement());
    EXPECT_TRUE(v.is_data_array());
}

// =========================================================================
//  Accessors — happy path
// =========================================================================

TEST(ValueTest, AsMeasurementReturnsCorrectValue)
{
    Value v(make_scalar_real(2.718));
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 2.718);
}

TEST(ValueTest, AsMeasurementConstWorks)
{
    const Value v(make_scalar_real(2.718));
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 2.718);
}

TEST(ValueTest, AsDataArrayReturnsSameObject)
{
    auto da = make_data_array();
    Value v(da);
    EXPECT_EQ(&v.as_data_array(), da.get());
}

TEST(ValueTest, AsDataArrayConstWorks)
{
    auto da = make_data_array();
    const Value v(da);
    EXPECT_EQ(&v.as_data_array(), da.get());
}

// =========================================================================
//  Accessors — type mismatch
// =========================================================================

TEST(ValueTest, AsMeasurementOnNullThrows)
{
    Value v;
    EXPECT_THROW(v.as_measurement(), boost::bad_get);
}

TEST(ValueTest, AsMeasurementOnDataArrayThrows)
{
    Value v(make_data_array());
    EXPECT_THROW(v.as_measurement(), boost::bad_get);
}

TEST(ValueTest, AsDataArrayOnNullThrows)
{
    Value v;
    EXPECT_THROW(v.as_data_array(), boost::bad_get);
}

TEST(ValueTest, AsDataArrayOnMeasurementThrows)
{
    Value v(make_scalar_int(0));
    EXPECT_THROW(v.as_data_array(), boost::bad_get);
}

// =========================================================================
//  to_string
// =========================================================================

TEST(ValueTest, NullFormat)
{
    Value v;
    EXPECT_EQ(v.Format(), "NULL");
}

TEST(ValueTest, MeasurementScalarRealFormat)
{
    Value v(make_scalar_real(3.14));
    std::string s = v.Format();
    // Measurement::to_string() renders numeric value.
    EXPECT_NE(s.find("3.14"), std::string::npos);
}

TEST(ValueTest, MeasurementScalarIntegerFormat)
{
    Value v(make_scalar_int(42));
    std::string s = v.Format();
    EXPECT_NE(s.find("42"), std::string::npos);
}

// =========================================================================
//  Factory methods
// =========================================================================

TEST(ValueTest, FactoryNull)
{
    Value v = Value::Null();
    EXPECT_TRUE(v.is_null());
}

TEST(ValueTest, FactoryReal)
{
    Value v = Value::Real(3.14);
    EXPECT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kScalar);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kReal);
    EXPECT_DOUBLE_EQ(m.as_scalar<double>(), 3.14);
}

TEST(ValueTest, FactoryInteger)
{
    Value v = Value::Integer(42);
    EXPECT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kScalar);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(m.as_scalar<int>(), 42);
}

TEST(ValueTest, FactoryBooleanTrue)
{
    Value v = Value::BooleanValue(true);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(ValueTest, FactoryBooleanFalse)
{
    Value v = Value::BooleanValue(false);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(ValueTest, FactoryString)
{
    Value v = Value::String("hello");
    EXPECT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kScalar);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kString);
    EXPECT_EQ(m.as_scalar<std::string>(), "hello");
}
