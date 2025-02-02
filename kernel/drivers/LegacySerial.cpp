#include "kernel/drivers/LegacySerial.h"

LegacySerial::LegacySerial(DeviceAddress address) : LegacyDevice(address, DeviceClass::SERIAL)
{
    lock_init(_buffer_lock);
    com_initialize(port());
}

void LegacySerial::handle_interrupt()
{
    LockHolder holder(_buffer_lock);

    while (com_can_read(port()))
    {
        char byte = com_getc(port());
        _buffer.write((const char *)&byte, sizeof(byte));
    }
}

bool LegacySerial::can_read(FsHandle &handle)
{
    __unused(handle);

    // FIXME: make this atomic or something...
    return !_buffer.empty();
}

ResultOr<size_t> LegacySerial::read(FsHandle &handle, void *buffer, size_t size)
{
    __unused(handle);

    LockHolder holder(_buffer_lock);

    return _buffer.read((char *)buffer, size);
}

ResultOr<size_t> LegacySerial::write(FsHandle &handle, const void *buffer, size_t size)
{
    __unused(handle);

    LockHolder holder(_buffer_lock);

    return com_write(COM1, buffer, size);
}
