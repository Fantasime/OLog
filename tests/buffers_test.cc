#include "buffers.h"

#include <catch2/catch_test_macros.hpp>

#include "olog_config.h"

using namespace olog::buffers;

TEST_CASE("StagingBuffer should be destruct correctly", "[StagingBuffer::DestructGuard]") {
    StagingBuffer* bytes_pipe = nullptr;
    {
        StagingBuffer::DestructGuard guard;
        bytes_pipe = new StagingBuffer(0, 10, guard);
    }
    REQUIRE(bytes_pipe->shouldBeDestructed());

    delete bytes_pipe;
    bytes_pipe = nullptr;
}

TEST_CASE("Reservation equals capacity", "[StagingBuffer]") {
    const size_t BYTES_PIPE_CAPACITY = 512;
    StagingBuffer::DestructGuard guard;
    StagingBuffer bytes_pipe(0, BYTES_PIPE_CAPACITY, guard);
    REQUIRE(bytes_pipe.getCapacity() == BYTES_PIPE_CAPACITY);
    REQUIRE(bytes_pipe.reserveProducerSpace(BYTES_PIPE_CAPACITY, false) == nullptr);
}

TEST_CASE("Produce and consume synchronously", "[StagingBuffer]") {
    const size_t BYTES_PIPE_CAPACITY = 512;

    StagingBuffer::DestructGuard guard;
    StagingBuffer bytes_pipe(0, BYTES_PIPE_CAPACITY, guard);

    struct ObjectShouldBeStored {
        int int_val;
        char str[32];
        double double_val;
    };

    const int VALUE_OF_INT_VAL = 17;
    const char* VALUE_OF_STR = "Hello World";
    const double VALUE_OF_DOUBLE_VAL = 3.1415;

    char* write_pos = bytes_pipe.reserveProducerSpace(sizeof(ObjectShouldBeStored));
    ObjectShouldBeStored* object_in_storage = new (write_pos) ObjectShouldBeStored();
    object_in_storage->int_val = VALUE_OF_INT_VAL;
    memcpy(object_in_storage->str, VALUE_OF_STR, strlen(VALUE_OF_STR));
    object_in_storage->double_val = VALUE_OF_DOUBLE_VAL;
    bytes_pipe.finishReservation(sizeof(ObjectShouldBeStored));
    REQUIRE_FALSE(bytes_pipe.shouldBeDestructed());

    size_t available_bytes = 0;
    char* read_pos = bytes_pipe.peek(available_bytes);
    REQUIRE(available_bytes == sizeof(ObjectShouldBeStored));
    ObjectShouldBeStored* temp_object = reinterpret_cast<ObjectShouldBeStored*>(read_pos);
    ObjectShouldBeStored readed_object(*temp_object);

    REQUIRE(readed_object.int_val == VALUE_OF_INT_VAL);
    REQUIRE(strcmp(readed_object.str, VALUE_OF_STR) == 0);
    REQUIRE(readed_object.double_val == VALUE_OF_DOUBLE_VAL);

    bytes_pipe.consume(sizeof(ObjectShouldBeStored));
    read_pos = bytes_pipe.peek(available_bytes);
    REQUIRE(available_bytes == 0);
}

TEST_CASE("Should be destruct after consumption", "[StagingBuffer]") {
    const size_t BYTES_PIPE_CAPACITY = 512;

    StagingBuffer* bytes_pipe = nullptr;
    {
        StagingBuffer::DestructGuard guard;
        bytes_pipe = new StagingBuffer(0, BYTES_PIPE_CAPACITY, guard);
        REQUIRE_FALSE(bytes_pipe->shouldBeDestructed());

        int val = 11;
        char* write_pos = bytes_pipe->reserveProducerSpace(sizeof(val));
        memcpy(write_pos, &val, sizeof(val));
        bytes_pipe->finishReservation(sizeof(val));

        size_t available_bytes = 0;
        char* read_pos = bytes_pipe->peek(available_bytes);
        REQUIRE(available_bytes == sizeof(val));
        int readed_val = *reinterpret_cast<int*>(read_pos);
        REQUIRE(readed_val == val);
        bytes_pipe->consume(sizeof(readed_val));
        read_pos = bytes_pipe->peek(available_bytes);
        REQUIRE(available_bytes == 0);
        REQUIRE_FALSE(bytes_pipe->shouldBeDestructed());
    }

    REQUIRE(bytes_pipe->shouldBeDestructed());
    delete bytes_pipe;
}

TEST_CASE("Should not be destruct after incomplete consumption", "[StagingBuffer]") {
    const size_t BYTES_PIPE_CAPACITY = 512;

    StagingBuffer* bytes_pipe = nullptr;
    {
        StagingBuffer::DestructGuard guard;
        bytes_pipe = new StagingBuffer(0, BYTES_PIPE_CAPACITY, guard);
        REQUIRE_FALSE(bytes_pipe->shouldBeDestructed());

        int val = 11;
        char* write_pos = bytes_pipe->reserveProducerSpace(sizeof(val));
        memcpy(write_pos, &val, sizeof(val));
        bytes_pipe->finishReservation(sizeof(val));

        size_t available_bytes = 0;
        char* read_pos = bytes_pipe->peek(available_bytes);
        REQUIRE(available_bytes == sizeof(val));
        int readed_val = *reinterpret_cast<int*>(read_pos);
        REQUIRE(readed_val == val);

        read_pos = bytes_pipe->peek(available_bytes);
        REQUIRE(available_bytes == sizeof(val));
        REQUIRE_FALSE(bytes_pipe->shouldBeDestructed());
    }

    REQUIRE_FALSE(bytes_pipe->shouldBeDestructed());
    delete bytes_pipe;
}