#include "autotest.h"
#include "slicehandler.h"

#include "volumetesthelper.h"

using namespace udg;
using namespace testing;

class TestingSliceHandler : public SliceHandler {
public:
    void setSliceRange(int min, int max)
    {
        m_minSliceValue = min;
        m_numberOfSlices = max;
    }

    void setNumberOfPhases(int value)
    {
        m_numberOfPhases = value;
    }

public:
    bool m_sliceLoopEnabled;
    bool m_phaseLoopEnabled;

protected:
    bool isLoopEnabledForSlices() const
    {
        return m_sliceLoopEnabled;
    }

    bool isLoopEnabledForPhases() const
    {
        return m_phaseLoopEnabled;
    }
};

class test_SliceHandler : public QObject {
Q_OBJECT

private slots:
    void constructor_InitializesWithExpectedValues();

    void setVolume_UpdatesFieldsAsExpected();

    void setViewPlane_UpdatesViewPlaneAndSliceRange_data();
    void setViewPlane_UpdatesViewPlaneAndSliceRange();

    void setSlice_ComputesExpectedSlice_data();
    void setSlice_ComputesExpectedSlice();
    
    void setPhase_ComputesExpectedPhase_data();
    void setPhase_ComputesExpectedPhase();

    void setSlabThickness_ComputesExpectedThickness_data();
    void setSlabThickness_ComputesExpectedThickness();

    void getLastSlabSlice_ReturnsExpectedValue_data();
    void getLastSlabSlice_ReturnsExpectedValue();

    void setSlabThickness_ModifiesCurrentSliceAsExpected_data();
    void setSlabThickness_ModifiesCurrentSliceAsExpected();
};

Q_DECLARE_METATYPE(Volume*)
Q_DECLARE_METATYPE(OrthogonalPlane)

void test_SliceHandler::constructor_InitializesWithExpectedValues()
{
    SliceHandler sliceHandler;

    QCOMPARE(sliceHandler.getViewPlane(), OrthogonalPlane(OrthogonalPlane::XYPlane));
    QCOMPARE(sliceHandler.getCurrentSlice(), 0);
    QCOMPARE(sliceHandler.getMinimumSlice(), 0);
    QCOMPARE(sliceHandler.getMaximumSlice(), 0);
    QCOMPARE(sliceHandler.getCurrentPhase(), 0);
    QCOMPARE(sliceHandler.getNumberOfPhases(), 1);
    QCOMPARE(sliceHandler.getSlabThickness(), 1);
    QCOMPARE(sliceHandler.getLastSlabSlice(), 0);
}

void test_SliceHandler::setVolume_UpdatesFieldsAsExpected()
{
    TestingSliceHandler sliceHandler;
    sliceHandler.setViewPlane(OrthogonalPlane::XZPlane);
    sliceHandler.setSliceRange(-10, 10);
    sliceHandler.setSlice(5);
    sliceHandler.setNumberOfPhases(30);
    sliceHandler.setPhase(10);
    sliceHandler.setSlabThickness(2);

    Volume *volume = VolumeTestHelper::createVolume(20, 4, 5);
    sliceHandler.setVolume(volume);

    QCOMPARE(sliceHandler.getViewPlane(), OrthogonalPlane(OrthogonalPlane::XYPlane));
    QCOMPARE(sliceHandler.getCurrentSlice(), 0);
    QCOMPARE(sliceHandler.getMinimumSlice(), 0);
    QCOMPARE(sliceHandler.getMaximumSlice(), 4);
    QCOMPARE(sliceHandler.getCurrentPhase(), 0);
    QCOMPARE(sliceHandler.getNumberOfPhases(), 4);
    QCOMPARE(sliceHandler.getSlabThickness(), 1);
    QCOMPARE(sliceHandler.getLastSlabSlice(), 0);

    VolumeTestHelper::cleanUp(volume);
}

void test_SliceHandler::setViewPlane_UpdatesViewPlaneAndSliceRange_data()
{
    QTest::addColumn<Volume*>("volume");
    QTest::addColumn<OrthogonalPlane>("viewPlane");
    QTest::addColumn<int>("expectedMinimumSlice");
    QTest::addColumn<int>("expectedMaximumSlice");

    double origin[3] = { 0.0, 0.0, 0.0 };
    double spacing[3] = { 1.0, 1.0, 1.0 };
    int dimensions[3] = { 12, 16, 42 };
    int extent[6] = { 0, 11, 5, 20, 1, 42 };

    QTest::newRow("XY") << VolumeTestHelper::createVolumeWithParameters(1, 1, 1, origin, spacing, dimensions, extent)
                        << OrthogonalPlane(OrthogonalPlane::XYPlane) << 1 << 42;
    QTest::newRow("YZ") << VolumeTestHelper::createVolumeWithParameters(1, 1, 1, origin, spacing, dimensions, extent)
                        << OrthogonalPlane(OrthogonalPlane::YZPlane) << 0 << 11;
    QTest::newRow("XZ") << VolumeTestHelper::createVolumeWithParameters(1, 1, 1, origin, spacing, dimensions, extent)
                        << OrthogonalPlane(OrthogonalPlane::XZPlane) << 5 << 20;
}

void test_SliceHandler::setViewPlane_UpdatesViewPlaneAndSliceRange()
{
    QFETCH(Volume*, volume);
    QFETCH(OrthogonalPlane, viewPlane);
    QFETCH(int, expectedMinimumSlice);
    QFETCH(int, expectedMaximumSlice);

    SliceHandler sliceHandler;
    sliceHandler.setVolume(volume);
    sliceHandler.setViewPlane(viewPlane);

    QCOMPARE(sliceHandler.getMinimumSlice(), expectedMinimumSlice);
    QCOMPARE(sliceHandler.getMaximumSlice(), expectedMaximumSlice);

    VolumeTestHelper::cleanUp(volume);
}

void test_SliceHandler::setSlice_ComputesExpectedSlice_data()
{
    QTest::addColumn<bool>("loop");
    QTest::addColumn<int>("minSlice");
    QTest::addColumn<int>("maxSlice");
    QTest::addColumn<int>("sliceValue");
    QTest::addColumn<int>("expectedSliceValue");

    int minimumSlice = 0;
    int maximumSlice = 255;
    int sliceValueInsideRange = 10;
    QTest::newRow("No loop, value inside slice range") << false << minimumSlice << maximumSlice << sliceValueInsideRange << sliceValueInsideRange;
    QTest::newRow("Loop, value inside slice range") << true << minimumSlice << maximumSlice << sliceValueInsideRange << sliceValueInsideRange;
    
    int sliceValueBeyondMax = 600;
    QTest::newRow("No loop, value outside slice range (upper bound)") << false << minimumSlice << maximumSlice << sliceValueBeyondMax << maximumSlice;
    QTest::newRow("Loop, value outside slice range (upper bound)") << true << minimumSlice << maximumSlice << sliceValueBeyondMax << minimumSlice;

    int sliceValueBelowMin = -200;
    QTest::newRow("No loop, value outside slice range (lower bound)") << false << minimumSlice << maximumSlice << sliceValueBelowMin << minimumSlice;
    QTest::newRow("Loop, value outside slice range (lower bound)") << true << minimumSlice << maximumSlice << sliceValueBelowMin << maximumSlice;
}

void test_SliceHandler::setSlice_ComputesExpectedSlice()
{
    QFETCH(bool, loop);
    QFETCH(int, minSlice);
    QFETCH(int, maxSlice);
    QFETCH(int, sliceValue);
    QFETCH(int, expectedSliceValue);

    TestingSliceHandler sliceHandler;
    sliceHandler.m_sliceLoopEnabled = loop;
    sliceHandler.setSliceRange(minSlice, maxSlice);
    sliceHandler.setSlice(sliceValue);
    
    QCOMPARE(sliceHandler.getCurrentSlice(), expectedSliceValue);
}

void test_SliceHandler::setPhase_ComputesExpectedPhase_data()
{
    QTest::addColumn<bool>("loop");
    QTest::addColumn<int>("numberOfPhases");
    QTest::addColumn<int>("phaseValue");
    QTest::addColumn<int>("expectedPhaseValue");

    int numberOfPhases = 25;
    int phaseValueInsideRange = 10;
    QTest::newRow("No loop, value inside slice range") << false << numberOfPhases << phaseValueInsideRange << phaseValueInsideRange;
    QTest::newRow("Loop, value inside slice range") << true << numberOfPhases << phaseValueInsideRange << phaseValueInsideRange;
    
    int phaseValueBeyondMax = 600;
    QTest::newRow("No loop, value outside slice range (upper bound)") << false << numberOfPhases << phaseValueBeyondMax << numberOfPhases - 1;
    QTest::newRow("Loop, value outside slice range (upper bound)") << true << numberOfPhases << phaseValueBeyondMax << 0;

    int phaseValueBelowMin = -200;
    QTest::newRow("No loop, value outside slice range (lower bound)") << false << numberOfPhases << phaseValueBelowMin << 0;
    QTest::newRow("Loop, value outside slice range (lower bound)") << true << numberOfPhases << phaseValueBelowMin << numberOfPhases - 1;
}

void test_SliceHandler::setPhase_ComputesExpectedPhase()
{
    QFETCH(bool, loop);
    QFETCH(int, numberOfPhases);
    QFETCH(int, phaseValue);
    QFETCH(int, expectedPhaseValue);

    TestingSliceHandler sliceHandler;
    sliceHandler.m_phaseLoopEnabled = loop;
    sliceHandler.setNumberOfPhases(numberOfPhases);
    sliceHandler.setPhase(phaseValue);
    
    QCOMPARE(sliceHandler.getCurrentPhase(), expectedPhaseValue);
}

void test_SliceHandler::setSlabThickness_ComputesExpectedThickness_data()
{
    QTest::addColumn<int>("minSlice");
    QTest::addColumn<int>("maxSlice");
    QTest::addColumn<int>("slabThicknessValue");
    QTest::addColumn<int>("expectedSlabThicknessValue");

    int minimumSlice = 0;
    int maximumSlice = 255;
    QTest::newRow("Thickness inside range") << minimumSlice << maximumSlice << 200 << 200;
    QTest::newRow("Thickness greater than range") << minimumSlice << maximumSlice << 500 << 1;
}

void test_SliceHandler::setSlabThickness_ComputesExpectedThickness()
{
    QFETCH(int, minSlice);
    QFETCH(int, maxSlice);
    QFETCH(int, slabThicknessValue);
    QFETCH(int, expectedSlabThicknessValue);

    TestingSliceHandler sliceHandler;
    sliceHandler.setSliceRange(minSlice, maxSlice);
    sliceHandler.setSlabThickness(slabThicknessValue);
    
    QCOMPARE(sliceHandler.getSlabThickness(), expectedSlabThicknessValue);
}

void test_SliceHandler::getLastSlabSlice_ReturnsExpectedValue_data()
{
    QTest::addColumn<int>("minSlice");
    QTest::addColumn<int>("maxSlice");
    QTest::addColumn<int>("slice");
    QTest::addColumn<int>("slabThicknessValue");
    QTest::addColumn<int>("expectedLastSlabSlice");

    int minimumSlice = 0;
    int maximumSlice = 255;
    int slice = 10;
    int slabThickness = 200;
    QTest::newRow("Slab inside range") << minimumSlice << maximumSlice << slice << slabThickness << 110;
}

void test_SliceHandler::getLastSlabSlice_ReturnsExpectedValue()
{
    QFETCH(int, minSlice);
    QFETCH(int, maxSlice);
    QFETCH(int, slice);
    QFETCH(int, slabThicknessValue);
    QFETCH(int, expectedLastSlabSlice);

    TestingSliceHandler sliceHandler;
    sliceHandler.setSliceRange(minSlice, maxSlice);
    sliceHandler.setSlice(slice);
    sliceHandler.setSlabThickness(slabThicknessValue);
    
    QCOMPARE(sliceHandler.getLastSlabSlice(), expectedLastSlabSlice);
}

void test_SliceHandler::setSlabThickness_ModifiesCurrentSliceAsExpected_data()
{
    QTest::addColumn<int>("minSlice");
    QTest::addColumn<int>("maxSlice");
    QTest::addColumn<int>("slice");
    QTest::addColumn<int>("slabThicknessValue");
    QTest::addColumn<int>("expectedCurrentSlice");

    int minimumSlice = 0;
    int maximumSlice = 255;
    int slice = 50;
    int slabThickness = 25;
    QTest::newRow("Current slice decreases") << minimumSlice << maximumSlice << slice << slabThickness << 38;

    QTest::newRow("Current slice does not change") << minimumSlice << maximumSlice << slice << 2 << slice;
}

void test_SliceHandler::setSlabThickness_ModifiesCurrentSliceAsExpected()
{
    QFETCH(int, minSlice);
    QFETCH(int, maxSlice);
    QFETCH(int, slice);
    QFETCH(int, slabThicknessValue);
    QFETCH(int, expectedCurrentSlice);

    TestingSliceHandler sliceHandler;
    sliceHandler.setSliceRange(minSlice, maxSlice);
    sliceHandler.setSlice(slice);
    sliceHandler.setSlabThickness(slabThicknessValue);

    QCOMPARE(sliceHandler.getCurrentSlice(), expectedCurrentSlice);
}

DECLARE_TEST(test_SliceHandler)

#include "test_slicehandler.moc"
