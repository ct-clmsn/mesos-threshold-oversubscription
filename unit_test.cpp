#include "threshold_resource_estimator.hpp"
#include "threshold_qos_controller.hpp"

#include "testutils.hpp"
#include <stout/os.hpp>

#include <gtest/gtest.h>

#include "os.hpp"


using process::Future;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdQoSController;
using com::blue_yonder::os::MemInfo;

namespace {






struct ThresholdResourceEstimatorTests : public ::testing::Test {
    ResourceUsageFake usage;
    LoadFake load;
    MemInfoFake memory;
    ThresholdResourceEstimator estimator;

    ThresholdResourceEstimatorTests(
        std::string const & resources,
        os::Load const & loadThreshold,
        Bytes const & memThreshold
    ) :
        usage{},
        load{},
        memory{},
        estimator{
            load,
            memory,
            Resources::parse(resources).get(),
            loadThreshold,
            memThreshold}
    {
        estimator.initialize(usage);
    }
};


struct ThresholdQoSControllerTests : public ::testing::Test {
    ResourceUsageFake usage;
    LoadFake load;
    MemInfoFake memory;
    ThresholdQoSController controller;

    ThresholdQoSControllerTests(
        os::Load const & loadThreshold,
        Bytes const & memThreshold
    ) :
        usage{},
        load{},
        memory{},
        controller{
            load,
            memory,
            Resources::parse("").get(),
            loadThreshold,
            memThreshold}
    {
        controller.initialize(usage);
    }
};

struct NoResourcesTests : public ThresholdResourceEstimatorTests {
    NoResourcesTests() : ThresholdResourceEstimatorTests {
        "", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
    }
};

TEST_F(NoResourcesTests, noop) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}



struct EstimatorTests : public ThresholdResourceEstimatorTests {
    EstimatorTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128");
        load.set(3.9, 2.9, 1.9);
        memory.set("512MB", "64MB", "256MB");
    }
};

TEST_F(EstimatorTests, load_not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(EstimatorTests, load_exceeded) {
    load.set(10.0, 2.9, 1.9);
    auto available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());

    load.set(3.9, 10.0, 1.9);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());

    load.set(3.9, 2.9, 10.0);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, load_not_available) {
    load.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_exceeded) {
    memory.set("512MB", "0MB", "0MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_not_available) {
    memory.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}


struct ControllerTests : public ThresholdQoSControllerTests {
    ControllerTests() : ThresholdQoSControllerTests {
        os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128");
        load.set(3.9, 2.9, 1.9);
        memory.set("512MB", "64MB", "256MB");
    }
};

TEST_F(ControllerTests, load_not_exceeded) {
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.empty());
}

TEST_F(ControllerTests, load_exceeded) {
    load.set(10.0, 2.9, 1.9);
    auto corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);

    load.set(3.9, 10.0, 1.9);
    corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);

    load.set(3.9, 2.9, 10.0);
    corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, load_not_available) {
    load.set_error();
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, mem_exceeded) {
    memory.set("512MB", "0MB", "0MB");
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, mem_not_available) {
    memory.set_error();
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, thresholds_exceed_but_no_revocable_tasks) {
    load.set(10.0, 2.9, 1.9);
    usage.set("", "cpus(*):1.5;mem(*):128");
    memory.set("512MB", "32MB", "32MB");

    auto corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.empty());
}

}
