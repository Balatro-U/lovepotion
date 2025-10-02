#include "modules/sound/wrap_Sound.hpp"

#include "modules/data/DataStream.hpp"
#include "modules/filesystem/wrap_Filesystem.hpp"

#include "modules/sound/wrap_Decoder.hpp"
#include "modules/sound/wrap_SoundData.hpp"

using namespace love;

#define instance() (Module::getInstance<Sound>(Module::M_SOUND))

int Wrap_Sound::newDecoder(lua_State* L)
{
#ifdef __WIIU__
    FILE* logFile = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile) {
        fprintf(logFile, "Wrap_Sound::newDecoder() called with %d arguments\n", lua_gettop(L));
        for (int i = 1; i <= lua_gettop(L); i++) {
            fprintf(logFile, "  Argument %d type: %d\n", i, lua_type(L, i));
            if (lua_type(L, i) == LUA_TSTRING) {
                fprintf(logFile, "  Argument %d string: %s\n", i, lua_tostring(L, i));
            } else if (lua_type(L, i) == LUA_TNUMBER) {
                fprintf(logFile, "  Argument %d number: %f\n", i, lua_tonumber(L, i));
            }
        }
        fflush(logFile);
        fclose(logFile);
    }
#endif

    int bufferSize = luaL_optinteger(L, 2, Decoder::DEFAULT_BUFFER_SIZE);
    Stream* stream = nullptr;

#ifdef __WIIU__
    FILE* logFile_check = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_check) {
        fprintf(logFile_check, "newDecoder() - bufferSize: %d, checking if can get file\n", bufferSize);
        fflush(logFile_check);
        fclose(logFile_check);
    }
#endif

    if (luax_cangetfile(L, 1))
    {
#ifdef __WIIU__
        FILE* logFile_file = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_file) {
            fprintf(logFile_file, "newDecoder() - can get file, processing file input\n");
            fflush(logFile_file);
            fclose(logFile_file);
        }
#endif
        Decoder::StreamSource source = Decoder::STREAM_FILE;
        const char* sourceType       = lua_isnoneornil(L, 3) ? nullptr : luaL_checkstring(L, 3);

        if (sourceType != nullptr && !Decoder::getConstant(sourceType, source))
            return luax_enumerror(L, "stream type", Decoder::StreamSources, sourceType);

        if (source == Decoder::STREAM_FILE)
        {
#ifdef __WIIU__
            FILE* logFile_file2 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile_file2) {
                fprintf(logFile_file2, "newDecoder() - about to get file and open for reading\n");
                fflush(logFile_file2);
                fclose(logFile_file2);
            }
#endif
            auto* file = luax_getfile(L, 1);
            luax_catchexcept(L, [&]() { file->open(File::MODE_READ); });
            stream = file;
#ifdef __WIIU__
            FILE* logFile_file3 = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile_file3) {
                fprintf(logFile_file3, "newDecoder() - file opened successfully, stream: %p\n", stream);
                fflush(logFile_file3);
                fclose(logFile_file3);
            }
#endif
        }
        else
        {
#ifdef __WIIU__
            FILE* logFile_data = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
            if (logFile_data) {
                fprintf(logFile_data, "newDecoder() - creating DataStream from file data\n");
                fflush(logFile_data);
                fclose(logFile_data);
            }
#endif
            luax_catchexcept(L, [&]() {
                StrongRef<FileData> data(luax_getfiledata(L, 1), Acquire::NO_RETAIN);
                stream = new DataStream(data);
            });
        }
    }
    else if (luax_istype(L, 1, Data::type))
    {
#ifdef __WIIU__
        FILE* logFile_dtype = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_dtype) {
            fprintf(logFile_dtype, "newDecoder() - processing Data type input\n");
            fflush(logFile_dtype);
            fclose(logFile_dtype);
        }
#endif
        Data* data = luax_checktype<Data>(L, 1);
        luax_catchexcept(L, [&]() { stream = new DataStream(data); });
    }
    else
    {
#ifdef __WIIU__
        FILE* logFile_stream = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
        if (logFile_stream) {
            fprintf(logFile_stream, "newDecoder() - processing Stream type input\n");
            fflush(logFile_stream);
            fclose(logFile_stream);
        }
#endif
        stream = luax_checktype<Stream>(L, 1);
        stream->retain();
    }

    Decoder* decoder = nullptr;

#ifdef __WIIU__
    FILE* logFile_create = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_create) {
        fprintf(logFile_create, "newDecoder() - about to create decoder with stream: %p, bufferSize: %d\n", stream, bufferSize);
        fflush(logFile_create);
        fclose(logFile_create);
    }
#endif

    // clang-format off
    luax_catchexcept(L, [&]() {
        decoder = instance()->newDecoder(stream, bufferSize); },
        [&](bool) { stream->release(); }
    );
    // clang-format on

#ifdef __WIIU__
    FILE* logFile_result = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_result) {
        fprintf(logFile_result, "newDecoder() - decoder created: %p\n", decoder);
        fflush(logFile_result);
        fclose(logFile_result);
    }
#endif

    luax_pushtype(L, decoder);
    decoder->release();

#ifdef __WIIU__
    FILE* logFile_end = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log", "a");
    if (logFile_end) {
        fprintf(logFile_end, "newDecoder() - completed successfully\n");
        fflush(logFile_end);
        fclose(logFile_end);
    }
#endif

    return 1;
}

int Wrap_Sound::newSoundData(lua_State* L)
{
    SoundData* soundData = nullptr;

    if (lua_isnumber(L, 1))
    {
        int samples    = luaL_checkinteger(L, 1);
        int sampleRate = luaL_optinteger(L, 2, Decoder::DEFAULT_SAMPLE_RATE);
        int bitDepth   = luaL_optinteger(L, 3, Decoder::DEFAULT_BIT_DEPTH);
        int channels   = luaL_optinteger(L, 4, Decoder::DEFAULT_CHANNELS);

        luax_catchexcept(L, [&]() {
            soundData = instance()->newSoundData(samples, sampleRate, bitDepth, channels);
        });
    }
    else
    {
        if (!luax_istype(L, 1, Decoder::type))
        {
            Wrap_Sound::newDecoder(L);
            lua_replace(L, 1);
        }

        auto* decoder = luax_checkdecoder(L, 1);
        luax_catchexcept(L, [&]() { soundData = instance()->newSoundData(decoder); });
    }

    luax_pushtype(L, soundData);
    soundData->release();

    return 1;
}

// clang-format off
static constexpr luaL_Reg functions[] =
{
    { "newDecoder",   Wrap_Sound::newDecoder   },
    { "newSoundData", Wrap_Sound::newSoundData }
};

static constexpr lua_CFunction types[] =
{
    open_sounddata,
    open_decoder
};
// clang-format on

int Wrap_Sound::open(lua_State* L)
{
    Sound* instance = instance();

    if (instance == nullptr)
        luax_catchexcept(L, [&]() { instance = new Sound(); });
    else
        instance->retain();

    WrappedModule module {};
    module.instance  = instance;
    module.name      = "sound";
    module.type      = &Module::type;
    module.functions = functions;
    module.types     = types;

    return luax_register_module(L, module);
}
