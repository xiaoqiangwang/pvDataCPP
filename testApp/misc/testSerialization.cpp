/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/*
 * testSerialization.cpp
 *
 *  Created on: Oct 25, 2010
 *      Author: Miha Vitorovic
 */
#include <iostream>
#include <fstream>

#include <epicsUnitTest.h>
#include <testMain.h>
#include <dbDefs.h> // for NELEMENTS

#include <epicsExit.h>
#include <pv/pvIntrospect.h>
#include <pv/pvData.h>
#include <pv/serialize.h>
#include <pv/noDefaultMethods.h>
#include <pv/byteBuffer.h>
#include <pv/convert.h>

#include <pv/standardField.h>

#include <limits>

#define BYTE_MAX_VALUE std::numeric_limits<int8>::max()
#define BYTE_MIN_VALUE std::numeric_limits<int8>::min()
#define UBYTE_MAX_VALUE std::numeric_limits<uint8>::max()
#define SHORT_MAX_VALUE std::numeric_limits<int16>::max()
#define SHORT_MIN_VALUE std::numeric_limits<int16>::min()
#define USHORT_MAX_VALUE std::numeric_limits<uint16>::max()
#define INT_MAX_VALUE std::numeric_limits<int32>::max()
#define INT_MIN_VALUE std::numeric_limits<int32>::min()
#define UINT_MAX_VALUE std::numeric_limits<uint32>::max()
#define LONG_MAX_VALUE std::numeric_limits<int64>::max()
#define LONG_MIN_VALUE std::numeric_limits<int64>::min()
#define ULONG_MAX_VALUE std::numeric_limits<uint64>::max()
#define FLOAT_MAX_VALUE std::numeric_limits<float>::max()
#define FLOAT_MIN_VALUE std::numeric_limits<float>::min()
#define DOUBLE_MAX_VALUE std::numeric_limits<double>::max()
#define DOUBLE_MIN_VALUE std::numeric_limits<double>::min()

using namespace epics::pvData;

namespace {

static SerializableControl* flusher;
static DeserializableControl* control;
static ByteBuffer* buffer;


class SerializableControlImpl : public SerializableControl,
        public NoDefaultMethods {
public:
    virtual void flushSerializeBuffer() {
    }

    virtual void ensureBuffer(std::size_t /*size*/) {
    }

    virtual void alignBuffer(std::size_t alignment) {
        buffer->align(alignment);
    }
    
    virtual bool directSerialize(ByteBuffer* /*existingBuffer*/, const char* /*toSerialize*/,
                                 std::size_t /*elementCount*/, std::size_t /*elementSize*/)
    {
        return false;
    }

    virtual void cachedSerialize(std::tr1::shared_ptr<const Field> const & field, ByteBuffer* buffer)
    {
        field->serialize(buffer, this);
    }

    SerializableControlImpl() {
    }

    virtual ~SerializableControlImpl() {
    }
};

class DeserializableControlImpl : public DeserializableControl,
        public NoDefaultMethods {
public:
    virtual void ensureData(size_t /*size*/) {
    }

    virtual void alignData(size_t alignment) {
        buffer->align(alignment);
    }

    virtual bool directDeserialize(ByteBuffer* /*existingBuffer*/, char* /*deserializeTo*/,
                                   std::size_t /*elementCount*/, std::size_t /*elementSize*/)
    {
        return false;
    }

    virtual std::tr1::shared_ptr<const Field> cachedDeserialize(ByteBuffer* buffer)
    {
        return getFieldCreate()->deserialize(buffer, this);
    }

    DeserializableControlImpl() {
    }

    virtual ~DeserializableControlImpl() {
    }
};

void serializationTest(PVFieldPtr const & field) {
    buffer->clear();

    // serialize
    field->serialize(buffer, flusher);

    buffer->flip();

    // create new instance and deserialize
    PVFieldPtr deserializedField = getPVDataCreate()->createPVField(field->getField());
    deserializedField->deserialize(buffer, control);

    // must equal
    if(*field==*deserializedField)
        testPass("Serialization round trip OK");
    else {
        testFail("Serialization round trip did not match!");
        std::string buf;
        field->toString(&buf);
        testDiag("Expected: %s", buf.c_str());
        buf.clear();
        deserializedField->toString(&buf);
        testDiag("Found: %s", buf.c_str());
    }
}

void testEquals() {
    testDiag("Testing equals...");  // and non-initialized
    PVDataCreatePtr factory = getPVDataCreate();
    testOk1(factory.get()!=NULL);

	 // be sure all is covered
	 for (int i = pvBoolean; i < pvString; i++)
	 {
		 ScalarType scalarType = static_cast<ScalarType>(i);

		 PVScalarPtr scalar1 = factory->createPVScalar(scalarType);
		 PVScalarPtr scalar2 = factory->createPVScalar(scalarType);
		 testOk1((*scalar1)==(*scalar2));

		 PVScalarArrayPtr array1 = factory->createPVScalarArray(scalarType);
		 PVScalarArrayPtr array2 = factory->createPVScalarArray(scalarType);
		 testOk1((*array1)==(*array2));
	}

	// and a structure
    PVStructurePtr structure1 = factory->createPVStructure(getStandardField()->timeStamp());
    PVStructurePtr structure2 = factory->createPVStructure(getStandardField()->timeStamp());
	testOk1((*structure1)==(*structure2));

    // and a structure array
    PVStructureArrayPtr structureArray1 = factory->createPVStructureArray(getFieldCreate()->createStructureArray(structure1->getStructure()));
    PVStructureArrayPtr structureArray2 = factory->createPVStructureArray(getFieldCreate()->createStructureArray(structure2->getStructure()));
	testOk1((*structureArray1)==(*structureArray2));
	
	// variant union
	PVUnionPtr variantUnion1 = factory->createPVVariantUnion();
	PVUnionPtr variantUnion2 = factory->createPVVariantUnion();
	testOk1((*variantUnion1)==(*variantUnion2));
	
	variantUnion1->set(structure1);
	variantUnion2->set(structure1);
	testOk1((*variantUnion1)==(*variantUnion2));

	variantUnion2->set(structureArray1);
	testOk1((*variantUnion1)!=(*variantUnion2));
	
	// variant union array
	PVUnionArrayPtr variantUnionArray1 = factory->createPVVariantUnionArray();
	PVUnionArrayPtr variantUnionArray2 = factory->createPVVariantUnionArray();
	testOk1((*variantUnionArray1)==(*variantUnionArray2));

    // union	
    UnionConstPtr punion = getFieldCreate()->createFieldBuilder()->
                            add("double", pvDouble)->
                            add("double2", pvDouble)->
                            addNestedStructureArray("nested")->
                                setId("nestedId")->
                                add("short", pvShort)->
                                add("long", pvLong)->
                                addNested()->
                            addArray("intArray", pvInt)->
                            createUnion();
	PVUnionPtr union1 = factory->createPVUnion(punion);
	PVUnionPtr union2 = factory->createPVUnion(punion);
	testOk1((*union1)==(*union2));
	
	union1->select<PVDouble>("double")->put(1.2);
	union2->select<PVDouble>("double")->put(1.2);
	testOk1((*union1)==(*union2));
	
	union2->select<PVDouble>("double")->put(2.2);
	testOk1((*union1)!=(*union2));

	union2->select<PVDouble>("double2")->put(1.2);
	testOk1((*union1)!=(*union2));

	union2->select("nested");
	testOk1((*union1)!=(*union2));

	testOk1((*union1)!=(*variantUnion2));
	
	PVUnionArrayPtr unionArray1 = factory->createPVUnionArray(getFieldCreate()->createUnionArray(punion));
	PVUnionArrayPtr unionArray2 = factory->createPVUnionArray(getFieldCreate()->createUnionArray(punion));
	testOk1((*unionArray1)==(*unionArray2));

	testOk1((*variantUnionArray1)!=(*unionArray2));
}

template<typename PVT>
void testScalarType()
{
    typedef typename PVT::value_type value_type;

    testDiag("type %s", ScalarTypeFunc::name(PVT::typeCode));

    typename PVT::shared_pointer pv = std::tr1::static_pointer_cast<PVT>(getPVDataCreate()->createPVScalar(PVT::typeCode));

    pv->put(0);
    serializationTest(pv);
    pv->put(42);
    serializationTest(pv);
    pv->put(std::numeric_limits<value_type>::max()-1);
    serializationTest(pv);
    pv->put(std::numeric_limits<value_type>::max());
    serializationTest(pv);

    if(std::numeric_limits<value_type>::min()!=0) {
        pv->put(-42);
        serializationTest(pv);
        pv->put(std::numeric_limits<value_type>::min()+1);
        serializationTest(pv);
        pv->put(std::numeric_limits<value_type>::min());
        serializationTest(pv);
    }

    if(std::numeric_limits<value_type>::has_infinity) {
        pv->put(std::numeric_limits<value_type>::infinity());
        serializationTest(pv);
    }
}

void testScalar() {
    testDiag("Testing scalars...");
    PVDataCreatePtr factory = getPVDataCreate();
    testOk1(factory.get()!=NULL);

    testDiag("type %s", ScalarTypeFunc::name(pvBoolean));
    PVBooleanPtr pvBoolean =
    		std::tr1::static_pointer_cast<PVBoolean>(factory->createPVScalar(epics::pvData::pvBoolean));
    pvBoolean->put(false);
    serializationTest(pvBoolean);
    pvBoolean->put(true);
    serializationTest(pvBoolean);

    testScalarType<PVByte>();
    testScalarType<PVUByte>();
    testScalarType<PVShort>();
    testScalarType<PVUShort>();
    testScalarType<PVInt>();
    testScalarType<PVUInt>();
    testScalarType<PVLong>();
    testScalarType<PVULong>();
    testScalarType<PVFloat>();
    testScalarType<PVDouble>();


    testDiag("type %s", ScalarTypeFunc::name(pvString));
    PVStringPtr pvString =
    		std::tr1::static_pointer_cast<PVString>(factory->createPVScalar(epics::pvData::pvString));
    pvString->put("");
    serializationTest(pvString);
    pvString->put("s");
    serializationTest(pvString);
    pvString->put("string");
    serializationTest(pvString);
    pvString->put("string with spaces");
    serializationTest(pvString);
    pvString->put("string with spaces and special characters\f\n");
    serializationTest(pvString);

    // huge string test
    pvString->put(String(10000, 'a'));
    serializationTest(pvString);
}

template<typename PVT>
void testArrayType(const typename PVT::value_type* rdata, size_t len)
{
    typedef typename PVT::value_type value_type;

    typename PVT::svector empty(0), data(len);

    std::copy(rdata, rdata+len, data.begin());

    testDiag("type %s", ScalarTypeFunc::name(PVT::typeCode));

    typename PVT::shared_pointer pv = std::tr1::static_pointer_cast<PVT>(getPVDataCreate()->createPVScalarArray(PVT::typeCode));

    pv->replace(freeze(empty));
    serializationTest(pv);
    pv->replace(freeze(data));
    serializationTest(pv);
}

static const boolean bdata[] = {0, 1, 0, 1, 1};

static const int8 i8data[] = { 0, 1, 2, -1, BYTE_MAX_VALUE, BYTE_MAX_VALUE-1,
                               BYTE_MIN_VALUE+1, BYTE_MIN_VALUE };
static const uint8 u8data[] = { 0, 1, 2, -1, UBYTE_MAX_VALUE, UBYTE_MAX_VALUE-1 };

static const int16 i16data[] = { 0, 1, 2, -1, SHORT_MAX_VALUE, SHORT_MAX_VALUE-1,
                               SHORT_MIN_VALUE+1, SHORT_MIN_VALUE };
static const uint16 u16data[] = { 0, 1, 2, -1, USHORT_MAX_VALUE, USHORT_MAX_VALUE-1 };

static const int32 i32data[] = { 0, 1, 2, -1, INT_MAX_VALUE, INT_MAX_VALUE-1,
                               INT_MIN_VALUE+1, INT_MIN_VALUE };
static const uint32 u32data[] = { 0, 1, 2, -1, UINT_MAX_VALUE, UINT_MAX_VALUE-1 };

static const int64 i64data[] = { 0, 1, 2, -1, LONG_MAX_VALUE, LONG_MAX_VALUE-1,
                               LONG_MIN_VALUE+1, LONG_MIN_VALUE };
static const uint64 u64data[] = { 0, 1, 2, -1, ULONG_MAX_VALUE, ULONG_MAX_VALUE-1 };

static const double ddata[] = { (double)0.0, (double)1.1, (double)2.3, (double)-1.4,
                                DOUBLE_MAX_VALUE, DOUBLE_MAX_VALUE-(double)123456.789,
                                DOUBLE_MIN_VALUE+(double)1.1, DOUBLE_MIN_VALUE };

static const float fdata[] = { (float)0.0, (float)1.1, (float)2.3, (float)-1.4,
                               FLOAT_MAX_VALUE, FLOAT_MAX_VALUE-(float)123456.789,
                               FLOAT_MIN_VALUE+(float)1.1, FLOAT_MIN_VALUE };

static const String sdata[] = {
    "",
    "a",
    "a b",
    " ",
    "test",
    "smile",
    "this is a little longer string... maybe a little but longer... this makes test better",
    String(10000, 'b')
};

void testArray() {
    testDiag("Testing arrays...");

    testArrayType<PVBooleanArray>(bdata, NELEMENTS(bdata));

    testArrayType<PVByteArray>(i8data, NELEMENTS(i8data));
    testArrayType<PVUByteArray>(u8data, NELEMENTS(u8data));
    testArrayType<PVShortArray>(i16data, NELEMENTS(i16data));
    testArrayType<PVUShortArray>(u16data, NELEMENTS(u16data));
    testArrayType<PVIntArray>(i32data, NELEMENTS(i32data));
    testArrayType<PVUIntArray>(u32data, NELEMENTS(u32data));
    testArrayType<PVLongArray>(i64data, NELEMENTS(i64data));
    testArrayType<PVULongArray>(u64data, NELEMENTS(u64data));

    testArrayType<PVDoubleArray>(ddata, NELEMENTS(ddata));
    testArrayType<PVFloatArray>(fdata, NELEMENTS(fdata));

    testArrayType<PVStringArray>(sdata, NELEMENTS(sdata));
}

void testStructure() {
    testDiag("Testing structure...");

    PVDataCreatePtr factory = getPVDataCreate();
    testOk1(factory.get()!=NULL);

    testDiag("\tSimple structure serialization");
	PVStructurePtr pvStructure = factory->createPVStructure(getStandardField()->timeStamp());
    pvStructure->getLongField("secondsPastEpoch")->put(123);
    pvStructure->getIntField("nanoSeconds")->put(456);
    pvStructure->getIntField("userTag")->put(789);

    serializationTest(pvStructure);

    testDiag("\tComplex structure serialization");
	pvStructure = factory->createPVStructure(
			getStandardField()->structureArray(
					getStandardField()->timeStamp(), "alarm,control,display,timeStamp")
			);
	// TODO fill with data
    serializationTest(pvStructure);
}

void testUnion() {
    testDiag("Testing union...");

    PVDataCreatePtr factory = getPVDataCreate();
    testOk1(factory.get()!=NULL);


    PVDoublePtr doubleValue = factory->createPVScalar<PVDouble>();
    PVIntPtr intValue = factory->createPVScalar<PVInt>();

    testDiag("\tVariant union test");

    PVUnionPtr variant = factory->createPVVariantUnion();
    testOk1(variant.get()!=NULL);
    testOk1(PVUnion::UNDEFINED_INDEX == variant->getSelectedIndex());
    testOk1("" == variant->getSelectedFieldName());
    serializationTest(variant);

    variant->set(doubleValue);
    testOk1(doubleValue.get() == variant->get().get());
    testOk1(PVUnion::UNDEFINED_INDEX == variant->getSelectedIndex());
    testOk1("" == variant->getSelectedFieldName());
    serializationTest(variant);

    variant->set(intValue);
    testOk1(intValue.get() == variant->get().get());
    testOk1(PVUnion::UNDEFINED_INDEX == variant->getSelectedIndex());
    testOk1("" == variant->getSelectedFieldName());
    serializationTest(variant);

    variant->set(PVUnion::UNDEFINED_INDEX, doubleValue);
    testOk1(doubleValue.get() == variant->get().get());
    testOk1(PVUnion::UNDEFINED_INDEX == variant->getSelectedIndex());

    variant->set(PVFieldPtr());
    testOk1(NULL == variant->get().get());

    testDiag("\tVariant union array test");

    PVUnionArrayPtr variantArray = factory->createPVVariantUnionArray();
    testOk1(variantArray.get()!=NULL);
    
    variantArray->setLength(6);
    PVUnionArray::svector data;

    PVUnionPtr u = factory->createPVVariantUnion();
    data.push_back(u);
  
    u = factory->createPVVariantUnion();
    u->set(factory->createPVStructure(getStandardField()->timeStamp()));
    data.push_back(u);
    
    u = factory->createPVVariantUnion();
    u->set(factory->createPVStructure(getStandardField()->control()));
    data.push_back(u);

    data.push_back(PVUnionPtr());

    variantArray->replace(freeze(data));
    serializationTest(variantArray);
    
    testDiag("\tVariant union test");
    
    UnionConstPtr punion = getFieldCreate()->createFieldBuilder()->
                            add("doubleValue", pvDouble)->
                            add("intValue", pvInt)->
                            createUnion();
                            
    u = factory->createPVUnion(punion);
    testOk1(NULL!=u.get());       

	// null union test
    testOk1(NULL==u->get().get());
    testOk1(PVUnion::UNDEFINED_INDEX == u->getSelectedIndex());
    testOk1("" == u->getSelectedFieldName());
    serializationTest(u);

    u->select<PVDouble>("doubleValue")->put(12);
    testOk1(12 == u->get<PVDouble>()->get());
    testOk1(0 == u->getSelectedIndex());
    testOk1("doubleValue" == u->getSelectedFieldName());
    serializationTest(u);
    
    u->select<PVInt>("intValue")->put(543);
    testOk1(543 == u->get<PVInt>()->get());
    testOk1(1 == u->getSelectedIndex());
    testOk1("intValue" == u->getSelectedFieldName());
    serializationTest(u);
    
    u->select<PVInt>(1)->put(5432);
    testOk1(5432 == u->get<PVInt>()->get());
    serializationTest(u);

    testOk1(NULL==u->select(PVUnion::UNDEFINED_INDEX).get());
    testOk1(NULL==u->get().get());
    testOk1(PVUnion::UNDEFINED_INDEX == u->getSelectedIndex());
    testOk1("" == u->getSelectedFieldName());
    serializationTest(u);
        
    u->set("doubleValue", doubleValue);
    testOk1(doubleValue.get() == u->get().get());
    testOk1(0 == u->getSelectedIndex());
    serializationTest(u);
    
    try
    {
        u->set(1, doubleValue);
        testFail("field type does not match, but set allowed");
    }
    catch (std::invalid_argument& ia)
    {
        // expected
        testPass("PVUnion.set(int32, PVFieldPtr const&) field type does not match test");
    }

    try
    {
        u->select(120);
        testFail("index out of bounds allowed");
    }
    catch (std::invalid_argument& ia)
    {
        // expected
        testPass("PVUnion.select(int32) index out of bounds test");
    }
    
    try
    {
        u->select(-2);
        testFail("index out of bounds allowed");
    }
    catch (std::invalid_argument& ia)
    {
        // expected
        testPass("PVUnion.select(int32) index out of bounds test");
    }

    try
    {
        u->set(120, doubleValue);
        testFail("index out of bounds allowed");
    }
    catch (std::invalid_argument& ia)
    {
        // expected
        testPass("PVUnion.set(int32, PVFieldPtr const&) index out of bounds test");
    }
    
    testDiag("\tUnion array test");

    PVUnionArrayPtr unionArray = factory->createPVUnionArray(getFieldCreate()->createUnionArray(punion));
    testOk1(unionArray.get()!=NULL);
    
    unionArray->setLength(6);
    data.clear();

    u = factory->createPVUnion(punion);
    data.push_back(u);
  
    u = factory->createPVUnion(punion);
    u->select<PVDouble>(0)->put(12);
    data.push_back(u);
    
    u = factory->createPVUnion(punion);
    u->select<PVInt>(1)->put(421);
    data.push_back(u);

    data.push_back(PVUnionPtr());

    unionArray->replace(freeze(data));
    serializationTest(unionArray);
    
}

void testStructureArray() {
    testDiag("Testing structure array...");

    PVDataCreatePtr factory = getPVDataCreate();
    testOk1(factory.get()!=NULL);

    StructureArrayConstPtr tstype(
            getFieldCreate()->createStructureArray(getStandardField()->alarm()));
    PVStructureArrayPtr pvArr = getPVDataCreate()->createPVStructureArray(tstype);

    testDiag("empty array");
    serializationTest(pvArr);

    pvArr->setLength(10);

    testDiag("All NULLs");
    serializationTest(pvArr);

    PVStructureArray::svector data(5);

    data[1] = getPVDataCreate()->createPVStructure(getStandardField()->alarm());
    data[4] = getPVDataCreate()->createPVStructure(getStandardField()->alarm());

    pvArr->replace(freeze(data));

    testDiag("Some NULLs");
    serializationTest(pvArr);
}


void testStructureId() {
    testDiag("Testing structureID...");

    FieldCreatePtr fieldCreate = getFieldCreate();

    StringArray fieldNames;
    fieldNames.push_back("longField");
    fieldNames.push_back("intField");

    FieldConstPtrArray fields;
    fields.push_back(fieldCreate->createScalar(pvLong));
    fields.push_back(fieldCreate->createScalar(pvInt));

    StructureConstPtr structureWithNoId = fieldCreate->createStructure(fieldNames, fields);
    StructureConstPtr structure1 = fieldCreate->createStructure("id1", fieldNames, fields);
    StructureConstPtr structure2 = fieldCreate->createStructure("id2", fieldNames, fields);


    testOk1(structureWithNoId!=structure1);
    testOk1(structure1!=structure2);

    //serializationTest(structure1);

    PVStructurePtr pvStructure = getPVDataCreate()->createPVStructure(structure1);
    serializationTest(pvStructure);
}

void serializatioTest(FieldConstPtr const & field)
{
	buffer->clear();

	// serialize
	field->serialize(buffer, flusher);

	// deserialize
	buffer->flip();

	FieldConstPtr deserializedField = getFieldCreate()->deserialize(buffer, control);

	// must equal
	testOk1(*field == *deserializedField);
}

void testIntrospectionSerialization()
{
	 testDiag("Testing introspection serialization...");

	 FieldCreatePtr factory = getFieldCreate();
	 testOk1(factory.get()!=NULL);

	 // be sure all is covered
	 for (int i = pvBoolean; i < pvString; i++)
	 {
		 ScalarType scalarType = static_cast<ScalarType>(i);

		 ScalarConstPtr scalar = factory->createScalar(scalarType);
		 serializatioTest(scalar);

		 ScalarArrayConstPtr array = factory->createScalarArray(scalarType);
		 serializatioTest(array);
	 }

     // and a structure
     StructureConstPtr structure = getStandardField()->timeStamp();
     serializatioTest(structure);

     // and a structure array
     StructureArrayConstPtr structureArray = factory->createStructureArray(structure);
     serializatioTest(structureArray);

     // variant union
     UnionConstPtr variant = factory->createVariantUnion();
     serializatioTest(variant);
     
     // variant array union
     UnionArrayConstPtr variantArray = factory->createVariantUnionArray();
     serializatioTest(variantArray);
     
     // union
     UnionConstPtr punion = factory->createFieldBuilder()->
                            add("double", pvDouble)->
                            addNestedStructureArray("nested")->
                                setId("nestedId")->
                                add("short", pvShort)->
                                add("long", pvLong)->
                                addNested()->
                            addArray("intArray", pvInt)->
                            createUnion();
     serializatioTest(punion);
     
     // union array
     UnionArrayConstPtr punionArray = factory->createUnionArray(punion);
     serializatioTest(punionArray);
}

void testStringCopy() {
    String s1 = "abc";
    String s2 = s1;
    if (s1.c_str() != s2.c_str())
        testDiag("implementation of epics::pvData::String assignment operator does not share content");
}

} // end namespace

MAIN(testSerialization) {

    testPlan(213);

    flusher = new SerializableControlImpl();
    control = new DeserializableControlImpl();
    buffer = new ByteBuffer(1<<16);

    testStringCopy();

    testIntrospectionSerialization();
    testEquals();

    testScalar();
    testArray();
    testStructure();
    testStructureArray();
    
    testUnion();


    delete buffer;
    delete control;
    delete flusher;

    epicsExitCallAtExits();
    return testDone();
}

