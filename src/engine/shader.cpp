// shader.cpp: OpenGL GLSL shader management

#include "engine.h"

struct GlobalShaderParamState : ShaderParamState
{
    uint version;

    GlobalShaderParamState() : version(0) {}
};

Shader *Shader::lastshader = NULL;

Shader *defaultshader = NULL, *rectshader = NULL, *cubemapshader = NULL, *notextureshader = NULL, *nocolorshader = NULL, *foggedshader = NULL, *foggednotextureshader = NULL, *stdworldshader = NULL, *lineshader = NULL, *foggedlineshader = NULL;

static hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
static GlobalShaderParamState vertexparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS], pixelparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS];
static bool standardshader = false, initshaders = false, forceshaders = true;
static uint paramversion = 0;

VAR(reservevpparams, 1, 16, 0);
VAR(maxtexcoords, 1, 0, 0);
VAR(maxvsuniforms, 1, 0, 0);
VAR(maxfsuniforms, 1, 0, 0);
VAR(maxvaryings, 1, 0, 0);
VAR(dbgshader, 0, 0, 2);

void loadshaders()
{
    GLint val;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &val);
    maxvsuniforms = val/4;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &val);
    maxfsuniforms = val/4;
    glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &val);
    maxvaryings = val;
    glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &val);
    maxtexcoords = val;

    initshaders = true;
    standardshader = true;
    execfile("data/glsl.cfg");
    standardshader = false;
    initshaders = false;
    defaultshader = lookupshaderbyname("default");
    stdworldshader = lookupshaderbyname("stdworld");
    if(!defaultshader || !stdworldshader) fatal("cannot find shader definitions");

    extern Slot dummyslot;
    dummyslot.shader = stdworldshader;

    rectshader = lookupshaderbyname("rect");
    cubemapshader = lookupshaderbyname("cubemap");
    notextureshader = lookupshaderbyname("notexture");
    nocolorshader = lookupshaderbyname("nocolor");
    foggedshader = lookupshaderbyname("fogged");
    foggednotextureshader = lookupshaderbyname("foggednotexture");
    lineshader = lookupshaderbyname("notexture");
    foggedlineshader = lookupshaderbyname("foggednotexture");
    
    defaultshader->set();
}

Shader *lookupshaderbyname(const char *name) 
{ 
    Shader *s = shaders.access(name);
    return s && s->detailshader ? s : NULL;
}

static void showglslinfo(GLhandleARB obj, const char *tname, const char *name, const char *source)
{
    GLint length = 0;
    glGetObjectParameteriv_(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
    if(length > 1)
    {
        GLcharARB *log = new GLcharARB[length];
        glGetInfoLog_(obj, length, &length, log);
        conoutf(CON_ERROR, "GLSL ERROR (%s:%s)", tname, name);
        puts(log);
        if(source) loopi(1000)
        {
            const char *next = strchr(source, '\n');
            printf("%d: ", i+1);
            fwrite(source, 1, next ? next - source + 1 : strlen(source), stdout); 
            if(!next) { putchar('\n'); break; }
            source = next + 1;
        } 
        delete[] log;
    }
}

static void compileglslshader(GLenum type, GLhandleARB &obj, const char *def, const char *tname, const char *name, bool msg = true) 
{
    const GLcharARB *source = (const GLcharARB*)(def + strspn(def, " \t\r\n")); 
    obj = glCreateShaderObject_(type);
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success) 
    {
        if(msg) showglslinfo(obj, tname, name, source);
        glDeleteObject_(obj);
        obj = 0;
    }
    else if(dbgshader > 1 && msg) showglslinfo(obj, tname, name, source);
}  

VAR(dbgubo, 0, 0, 1);

static void bindglsluniform(Shader &s, UniformLoc &u)
{
    u.loc = glGetUniformLocation_(s.program, u.name);
    if(!u.blockname) return;
    if(hasUBO)
    {
        GLuint bidx = glGetUniformBlockIndex_(s.program, u.blockname);
        GLuint uidx = GL_INVALID_INDEX;
        glGetUniformIndices_(s.program, 1, &u.name, &uidx);
        if(bidx != GL_INVALID_INDEX && uidx != GL_INVALID_INDEX)
        {
            GLint sizeval = 0, offsetval = 0, strideval = 0;
            glGetActiveUniformBlockiv_(s.program, bidx, GL_UNIFORM_BLOCK_DATA_SIZE, &sizeval);
            if(sizeval <= 0) return;
            glGetActiveUniformsiv_(s.program, 1, &uidx, GL_UNIFORM_OFFSET, &offsetval);
            if(u.stride > 0)
            {
                glGetActiveUniformsiv_(s.program, 1, &uidx, GL_UNIFORM_ARRAY_STRIDE, &strideval);
                if(strideval > u.stride) return;
            }
            u.offset = offsetval;
            u.size = sizeval;
            glUniformBlockBinding_(s.program, bidx, u.binding);
            if(dbgubo) conoutf(CON_DEBUG, "UBO: %s:%s:%d, offset: %d, size: %d, stride: %d", u.name, u.blockname, u.binding, offsetval, sizeval, strideval);
        }
    }
    else if(hasBUE)
    {
        GLint size = glGetUniformBufferSize_(s.program, u.loc), stride = 0;
        if(size <= 0) return;
        if(u.stride > 0)
        {
            defformatstring(elem1name)("%s[1]", u.name);
            GLint elem1loc = glGetUniformLocation_(s.program, elem1name);
            if(elem1loc == -1) return;
            GLintptr elem0off = glGetUniformOffset_(s.program, u.loc),
                     elem1off = glGetUniformOffset_(s.program, elem1loc);
            stride = elem1off - elem0off;
            if(stride > u.stride) return;
        }
        u.offset = 0;
        u.size = size;
        if(dbgubo) conoutf(CON_DEBUG, "BUE: %s:%s:%d, offset: %d, size: %d, stride: %d", u.name, u.blockname, u.binding, 0, size, stride);
    }
}

static void linkglslprogram(Shader &s, bool msg = true)
{
    s.program = s.vsobj && s.psobj ? glCreateProgramObject_() : 0;
    GLint success = 0;
    if(s.program)
    {
        glAttachObject_(s.program, s.vsobj);
        glAttachObject_(s.program, s.psobj);
        loopv(s.attriblocs)
        {
            AttribLoc &a = s.attriblocs[i];
            glBindAttribLocation_(s.program, a.loc, a.name);
        }
        glLinkProgram_(s.program);
        glGetObjectParameteriv_(s.program, GL_OBJECT_LINK_STATUS_ARB, &success);
    }
    if(success)
    {
        glUseProgramObject_(s.program);
        loopi(8)
        {
            defformatstring(arg)("tex%d", i);
            GLint loc = glGetUniformLocation_(s.program, arg);
            if(loc != -1) glUniform1i_(loc, i);
        }
        loopv(s.defaultparams)
        {
            ShaderParam &param = s.defaultparams[i];
            string pname;
            if(param.type==SHPARAM_UNIFORM) copystring(pname, param.name);
            else formatstring(pname)("%s%d", param.type==SHPARAM_VERTEX ? "v" : "p", param.index);
            param.loc = glGetUniformLocation_(s.program, pname);
        }
        loopv(s.uniformlocs) bindglsluniform(s, s.uniformlocs[i]);
        glUseProgramObject_(0);
    }
    else if(s.program)
    {
        if(msg) showglslinfo(s.program, "PROG", s.name, NULL);
        glDeleteObject_(s.program);
        s.program = 0;
    }
}

bool checkglslsupport()
{
#if 0
    /* check if GLSL profile supports loops
     */
    const GLcharARB *source = 
        "uniform int N;\n"
        "uniform vec4 delta;\n"
        "void main(void) {\n"
        "   vec4 test = vec4(0.0, 0.0, 0.0, 0.0);\n"
        "   for(int i = 0; i < N; i++)  test += delta;\n"
        "   gl_FragColor = test;\n"
        "}\n";
#else
    const GLcharARB *source =
        "void main(void) {\n"
        "   gl_FragColor = vec4(0.0);\n"
        "}\n";
#endif
    GLhandleARB obj = glCreateShaderObject_(GL_FRAGMENT_SHADER_ARB);
    if(!obj) return false;
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success)
    {
        glDeleteObject_(obj);
        return false;
    }
    GLhandleARB program = glCreateProgramObject_();
    if(!program)
    {
        glDeleteObject_(obj);
        return false;
    } 
    glAttachObject_(program, obj);
    glLinkProgram_(program); 
    glGetObjectParameteriv_(program, GL_OBJECT_LINK_STATUS_ARB, &success);
    glDeleteObject_(obj);
    glDeleteObject_(program);
    return success!=0;
}

#define ALLOCEXTPARAM 0xFF
#define UNUSEDEXTPARAM 0xFE
            
static int addextparam(Shader &s, const char *name, int type, int index, int loc)
{
    if(!(s.numextparams%4))
    {
        LocalShaderParamState *extparams = new LocalShaderParamState[s.numextparams+4];
        if(s.extparams)
        {
            memcpy(extparams, s.extparams, s.numextparams*sizeof(LocalShaderParamState));
            delete[] s.extparams;
        }
        s.extparams = extparams;
    }
    int extindex = s.numextparams;
    LocalShaderParamState &ext = s.extparams[extindex];
    ext.name = name;
    ext.type = type;
    ext.index = index;
    ext.loc = loc;
    s.numextparams++;
    return extindex;
}

static void allocglsluniformparam(Shader &s, int type, int index, bool local = false)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    int loc = val.name ? glGetUniformLocation_(s.program, val.name) : -1;
    if(loc == -1)
    {
        defformatstring(altname)("%s%d", type==SHPARAM_VERTEX ? "v" : "p", index);
        loc = glGetUniformLocation_(s.program, altname);
    }
    if(loc >= 0) loopi(s.numextparams)
    {
        LocalShaderParamState &ext = s.extparams[i];
        if(ext.loc != loc) continue;
        if(ext.type==SHPARAM_LOOKUP) 
        {
            ext.name = val.name;
            ext.type = type;
            ext.index = local ? -1 : index;
        }
        if(type==SHPARAM_VERTEX) s.extvertparams[index] = i;
        else s.extpixparams[index] = i;
        return;
    }
    if(loc == -1)
    {
        if(type==SHPARAM_VERTEX) s.extvertparams[index] = local ? UNUSEDEXTPARAM : ALLOCEXTPARAM;
        else s.extpixparams[index] = local ? UNUSEDEXTPARAM : ALLOCEXTPARAM;
        return;
    }
    int extindex = addextparam(s, val.name, type, local ? -1 : index, loc);
    if(type==SHPARAM_VERTEX) s.extvertparams[index] = extindex;
    else s.extpixparams[index] = extindex;
}

static void setglsluniformformat(Shader &s, const char *name, GLenum format, int size)
{
    switch(format)
    {
        case GL_FLOAT:
        case GL_FLOAT_VEC2_ARB:
        case GL_FLOAT_VEC3_ARB:
            break;
        case GL_FLOAT_VEC4_ARB:
        default:
            return;
    }
    if(size > 1 || !strncmp(name, "gl_", 3)) return;
    int loc = glGetUniformLocation_(s.program, name);
    if(loc < 0) return;
    loopvj(s.defaultparams) if(s.defaultparams[j].loc == loc)
    {
        s.defaultparams[j].format = format;
        return;
    }
    loopj(s.numextparams) if(s.extparams[j].loc == loc)
    {
        s.extparams[j].format = format;
        return;
    }
    int extindex = addextparam(s, NULL, SHPARAM_LOOKUP, -1, loc);
    if(extindex >= 0) s.extparams[extindex].format = format;
}

static void allocglslactiveuniforms(Shader &s)
{
    GLint numactive = 0;
    glGetObjectParameteriv_(s.program, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &numactive);
    string name;
    loopi(numactive)
    {
        GLsizei namelen = 0;
        GLint size = 0;
        GLenum format = GL_FLOAT_VEC4_ARB;
        name[0] = '\0';
        glGetActiveUniform_(s.program, i, sizeof(name)-1, &namelen, &size, &format, name);
        if(namelen <= 0) continue;
        name[clamp(int(namelen), 0, (int)sizeof(name)-2)] = '\0'; 
        setglsluniformformat(s, name, format, size);
    } 
}

static inline bool duplicateenvparam(GlobalShaderParamState &param)
{
    loopj(RESERVEDSHADERPARAMS) 
    {
        GlobalShaderParamState &vp = vertexparamstate[j];
        if(vp.name && !vp.local && vp.version > param.version && !strcmp(vp.name, param.name))
            return true;
        GlobalShaderParamState &pp = pixelparamstate[j];
        if(pp.name && !pp.local && pp.version > param.version && !strcmp(pp.name, param.name))
            return true;
    }
    return false;
}

void Shader::allocenvparams(Slot *slot)
{
    if(slot)
    {
#define UNIFORMTEX(name, tmu) \
        { \
            loc = glGetUniformLocation_(program, name); \
            int val = tmu; \
            if(loc != -1) glUniform1i_(loc, val); \
        }
        int loc, tmu = 2;
        if(type & SHADER_NORMALSLMS)
        {
            UNIFORMTEX("lmcolor", 1);
            UNIFORMTEX("lmdir", 2);
            tmu++;
        }
        else UNIFORMTEX("lightmap", 1);
        if(type & SHADER_ENVMAP) UNIFORMTEX("envmap", tmu++);
        UNIFORMTEX("shadowmap", 7);
        int stex = 0;
        loopv(slot->sts)
        {
            Slot::Tex &t = slot->sts[i];
            switch(t.type)
            {
                case TEX_DIFFUSE: UNIFORMTEX("diffusemap", 0); break;
                case TEX_NORMAL: UNIFORMTEX("normalmap", tmu++); break;
                case TEX_GLOW: UNIFORMTEX("glowmap", tmu++); break;
                case TEX_DECAL: UNIFORMTEX("decal", tmu++); break;
                case TEX_SPEC: if(t.combined<0) UNIFORMTEX("specmap", tmu++); break;
                case TEX_DEPTH: if(t.combined<0) UNIFORMTEX("depthmap", tmu++); break;
                case TEX_UNKNOWN: 
                {
                    defformatstring(sname)("stex%d", stex++); 
                    UNIFORMTEX(sname, tmu++);
                    break;
                }
            }
        }
    }
    if(!extvertparams) 
    {
        extvertparams = new uchar[2*RESERVEDSHADERPARAMS];
        extpixparams = extvertparams + RESERVEDSHADERPARAMS;
    }
    memset(extvertparams, ALLOCEXTPARAM, 2*RESERVEDSHADERPARAMS);
    loopi(RESERVEDSHADERPARAMS) if(vertexparamstate[i].name && !vertexparamstate[i].local && !duplicateenvparam(vertexparamstate[i]))
        allocglsluniformparam(*this, SHPARAM_VERTEX, i);
    loopi(RESERVEDSHADERPARAMS) if(pixelparamstate[i].name && !pixelparamstate[i].local && !duplicateenvparam(pixelparamstate[i]))
        allocglsluniformparam(*this, SHPARAM_PIXEL, i);
    allocglslactiveuniforms(*this);
}

static inline void setuniformval(LocalShaderParamState &l, const float *val)
{
    if(memcmp(l.curval, val, sizeof(l.curval)))
    {
        memcpy(l.curval, val, sizeof(l.curval));
        switch(l.format)
        {
            case GL_FLOAT:          glUniform1fv_(l.loc, 1, l.curval); break;
            case GL_FLOAT_VEC2_ARB: glUniform2fv_(l.loc, 1, l.curval); break;
            case GL_FLOAT_VEC3_ARB: glUniform3fv_(l.loc, 1, l.curval); break;
            case GL_FLOAT_VEC4_ARB: glUniform4fv_(l.loc, 1, l.curval); break;
        }
    }
}

static inline void flushparam(int type, int index)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    if(Shader::lastshader)
    {
        uchar &extindex = (type==SHPARAM_VERTEX ? Shader::lastshader->extvertparams[index] : Shader::lastshader->extpixparams[index]);
        if(extindex == ALLOCEXTPARAM) allocglsluniformparam(*Shader::lastshader, type, index, val.local);
        if(extindex < Shader::lastshader->numextparams)
            setuniformval(Shader::lastshader->extparams[extindex], val.val);
    }
}

static inline bool sortparamversions(const GlobalShaderParamState *x, const GlobalShaderParamState *y)
{
    return x->version < y->version;
}

static uint resetparamversions()
{
    GlobalShaderParamState *params[2*(RESERVEDSHADERPARAMS + MAXSHADERPARAMS)];
    loopi(RESERVEDSHADERPARAMS + MAXSHADERPARAMS)
    {
        params[2*i+0] = &vertexparamstate[i];  
        params[2*i+1] = &pixelparamstate[i];
    }
    quicksort(params, 2*(RESERVEDSHADERPARAMS + MAXSHADERPARAMS), sortparamversions);
    paramversion = 0;
    loopi(2*(RESERVEDSHADERPARAMS + MAXSHADERPARAMS)) params[i]->version = ++paramversion;
    return paramversion;
}
 
static inline ShaderParamState &setparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    GlobalShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    val.version = ++paramversion > 0 ? paramversion : resetparamversions();
    if(val.dirty==ShaderParamState::INVALID || val.val[0]!=x || val.val[1]!=y || val.val[2]!=z || val.val[3]!=w)
    {
        val.val[0] = x;
        val.val[1] = y;
        val.val[2] = z;
        val.val[3] = w;
        val.dirty = ShaderParamState::DIRTY;
    }
    return val;
}

static inline ShaderParamState &setparamfv(const char *name, int type, int index, const float *v)
{
    GlobalShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    val.version = ++paramversion > 0 ? paramversion : resetparamversions();
    if(val.dirty==ShaderParamState::INVALID || memcmp(val.val, v, sizeof(val.val)))
    {
        memcpy(val.val, v, sizeof(val.val));
        val.dirty = ShaderParamState::DIRTY;
    }
    return val;
}

void setenvparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = false;
}

void setenvparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = false;
}

void flushenvparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = false;
    flushparam(type, index);
}

void flushenvparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = false;
    flushparam(type, index);
}

void setlocalparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = true;
    flushparam(type, index);
}

void setlocalparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = true;
    flushparam(type, index);
}

void Shader::flushenvparams(Slot *slot)
{
    if(!used) allocenvparams(slot);
            
    loopi(numextparams)
    {
        LocalShaderParamState &ext = extparams[i];
        if(ext.index >= 0)
            setuniformval(ext, ext.type==SHPARAM_VERTEX ? vertexparamstate[ext.index].val : pixelparamstate[ext.index].val);
    }
    used = true;
}

static inline void setglslslotparam(const ShaderParam &p, LocalShaderParamState &l, uint &mask, int i)
{
    if(!(mask&(1<<i)))
    {
        mask |= 1<<i;
        setuniformval(l, p.val);
    }
}

static inline void setglslslotparams(vector<LocalShaderParamState> &defaultparams, Slot &slot, VSlot &vslot)
{
    uint unimask = 0;
    loopv(vslot.params)
    {
        ShaderParam &p = vslot.params[i];
        if(!defaultparams.inrange(p.loc)) continue;
        LocalShaderParamState &l = defaultparams[p.loc];
        setglslslotparam(p, l, unimask, p.loc);
    }
    loopv(slot.params)
    {
        ShaderParam &p = slot.params[i];
        if(!defaultparams.inrange(p.loc)) continue;
        LocalShaderParamState &l = defaultparams[p.loc];
        setglslslotparam(p, l, unimask, p.loc);
    }
    loopv(defaultparams)
    {
        LocalShaderParamState &l = defaultparams[i];
        setglslslotparam(l, l, unimask, i);
    }
}

void Shader::setslotparams(Slot &slot, VSlot &vslot)
{
    setglslslotparams(defaultparams, slot, vslot);
}

void Shader::bindprograms()
{
    if(this == lastshader || type&(SHADER_DEFERRED|SHADER_INVALID)) return;
    glUseProgramObject_(program);
    lastshader = this;
}

VARF(shaderprecision, 0, 0, 2, initwarning("shader quality"));

bool Shader::compile()
{
    if(!vsstr) vsobj = !reusevs || reusevs->type&SHADER_INVALID ? 0 : reusevs->vsobj;
    else compileglslshader(GL_VERTEX_SHADER_ARB,   vsobj, vsstr, "VS", name, dbgshader || !variantshader);
    if(!psstr) psobj = !reuseps || reuseps->type&SHADER_INVALID ? 0 : reuseps->psobj;
    else compileglslshader(GL_FRAGMENT_SHADER_ARB, psobj, psstr, "PS", name, dbgshader || !variantshader);
    linkglslprogram(*this, !variantshader);
    return program!=0;
}

void Shader::cleanup(bool invalid)
{
    detailshader = NULL;
    used = false;
    native = true;
    if(vsobj) { if(reusevs) glDeleteObject_(vsobj); vsobj = 0; }
    if(psobj) { if(reuseps) glDeleteObject_(psobj); psobj = 0; }
    if(program) { glDeleteObject_(program); program = 0; }
    numextparams = 0;
    DELETEA(extparams);
    DELETEA(extvertparams);
    extpixparams = NULL;
    loopv(defaultparams) memset(defaultparams[i].curval, -1, sizeof(defaultparams[i].curval));
    if(standard || invalid)
    {
        type = SHADER_INVALID;
        loopi(MAXVARIANTROWS) variants[i].setsize(0);
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        defaultparams.setsize(0);
        attriblocs.setsize(0);
        uniformlocs.setsize(0);
        altshader = NULL;
        loopi(MAXSHADERDETAIL) fastshader[i] = this;
        reusevs = reuseps = NULL;
    }
}

static void genattriblocs(Shader &s, const char *vs, const char *ps)
{
    static int len = strlen("#pragma CUBE2_attrib");
    string name;
    int loc;
    while((vs = strstr(vs, "#pragma CUBE2_attrib")))
    {
        if(sscanf(vs, "#pragma CUBE2_attrib %s %d", name, &loc) == 2)
            s.attriblocs.add(AttribLoc(getshaderparamname(name), loc));
        vs += len;
    }
}

static void genuniformlocs(Shader &s, const char *vs, const char *ps)
{
    static int len = strlen("#pragma CUBE2_uniform");
    string name, blockname;
    int binding, stride;
    while((vs = strstr(vs, "#pragma CUBE2_uniform")))
    {
        int numargs = sscanf(vs, "#pragma CUBE2_uniform %s %s %d %d", name, blockname, &binding, &stride);
        if(numargs >= 3) s.uniformlocs.add(UniformLoc(getshaderparamname(name), getshaderparamname(blockname), binding, numargs >= 4 ? stride : 0));
        else if(numargs >= 1) s.uniformlocs.add(UniformLoc(getshaderparamname(name)));
        vs += len;
    }
}

Shader *newshader(int type, const char *name, const char *vs, const char *ps, Shader *variant = NULL, int row = 0)
{
    if(Shader::lastshader)
    {
        glUseProgramObject_(0);
        Shader::lastshader = NULL;
    }

    Shader *exists = shaders.access(name); 
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.vsstr = newstring(vs);
    s.psstr = newstring(ps);
    DELETEA(s.defer);
    s.type = type;
    s.variantshader = variant;
    s.standard = standardshader;
    if(forceshaders) s.forced = true;
    s.reusevs = s.reuseps = NULL;
    if(variant)
    {
        int row = 0, col = 0;
        if(!vs[0] || sscanf(vs, "%d , %d", &row, &col) >= 1) 
        {
            DELETEA(s.vsstr);
            s.reusevs = !vs[0] ? variant : (variant->variants[row].inrange(col) ? variant->variants[row][col] : NULL);
        }
        row = col = 0;
        if(!ps[0] || sscanf(ps, "%d , %d", &row, &col) >= 1)
        {
            DELETEA(s.psstr);
            s.reuseps = !ps[0] ? variant : (variant->variants[row].inrange(col) ? variant->variants[row][col] : NULL);
        }
    }
    if(variant) loopv(variant->defaultparams) s.defaultparams.add(variant->defaultparams[i]);
    else loopv(curparams) s.defaultparams.add(curparams[i]);
    s.attriblocs.setsize(0);
    s.uniformlocs.setsize(0);
    genattriblocs(s, vs, ps);
    genuniformlocs(s, vs, ps);
    if(!s.compile())
    {
        s.cleanup(true);
        if(variant) shaders.remove(rname);
        return NULL;
    }
    if(variant) variant->variants[row].add(&s);
    s.fixdetailshader();
    return &s;
}

static const char *findglslmain(const char *s)
{
    const char *main = strstr(s, "main");
    if(!main) return NULL;
    for(; main >= s; main--) switch(*main) { case '\r': case '\n': case ';': return main + 1; }
    return s;
}

static void gengenericvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row)
{
    bool vschanged = false, pschanged = false;
    vector<char> vsv, psv;
    vsv.put(vs, strlen(vs)+1);
    psv.put(ps, strlen(ps)+1);

    static const int len = strlen("#pragma CUBE2_variant"), olen = strlen("override");
    for(char *vspragma = vsv.getbuf();; vschanged = true)
    {
        vspragma = strstr(vspragma, "#pragma CUBE2_variant");
        if(!vspragma) break;
        memset(vspragma, ' ', len);
        vspragma += len;
        if(!strncmp(vspragma, "override", olen))
        { 
            memset(vspragma, ' ', olen);
            vspragma += olen;
            char *end = vspragma + strcspn(vspragma, "\n\r");
            int endlen = strspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    for(char *pspragma = psv.getbuf();; pschanged = true)
    {
        pspragma = strstr(pspragma, "#pragma CUBE2_variant");
        if(!pspragma) break;
        memset(pspragma, ' ', len);
        pspragma += len;
        if(!strncmp(pspragma, "override", olen))
        { 
            memset(pspragma, ' ', olen);
            pspragma += olen;
            char *end = pspragma + strcspn(pspragma, "\n\r");
            int endlen = strspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    defformatstring(varname)("<variant:%d,%d>%s", s.variants[row].length(), row, sname);
    defformatstring(reuse)("%d", row);
    newshader(s.type, varname, vschanged ? vsv.getbuf() : reuse, pschanged ? psv.getbuf() : reuse, &s, row);
}

static void genuniformdefs(vector<char> &vsbuf, vector<char> &psbuf, const char *vs, const char *ps, Shader *variant = NULL)
{
    if(variant ? variant->defaultparams.empty() : curparams.empty()) return;
    const char *vsmain = findglslmain(vs), *psmain = findglslmain(ps);
    if(!vsmain || !psmain) return;
    vsbuf.put(vs, vsmain - vs);
    psbuf.put(ps, psmain - ps);
    if(variant) loopv(variant->defaultparams)
    {
        defformatstring(uni)("\nuniform vec4 %s;\n", variant->defaultparams[i].name);
        vsbuf.put(uni, strlen(uni));
        psbuf.put(uni, strlen(uni));
    }
    else loopv(curparams)
    {
        defformatstring(uni)("\nuniform vec4 %s;\n", curparams[i].name);
        vsbuf.put(uni, strlen(uni));
        psbuf.put(uni, strlen(uni));
    }
    vsbuf.put(vsmain, strlen(vsmain)+1);
    psbuf.put(psmain, strlen(psmain)+1);
}

VAR(defershaders, 0, 1, 1);

void defershader(int *type, const char *name, const char *contents)
{
    Shader *exists = shaders.access(name);
    if(exists && !(exists->type&SHADER_INVALID)) return;
    if(!defershaders) { execute(contents); return; }
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    DELETEA(s.defer);
    s.defer = newstring(contents);
    s.type = SHADER_DEFERRED | *type;
    s.standard = standardshader;
}

void useshader(Shader *s)
{
    if(!(s->type&SHADER_DEFERRED) || !s->defer) return;
        
    char *defer = s->defer;
    s->defer = NULL;
    bool wasstandard = standardshader, wasforcing = forceshaders;
    int oldflags = identflags;
    standardshader = s->standard;
    forceshaders = false;
    identflags &= ~IDF_PERSIST;
    curparams.shrink(0);
    execute(defer);
    identflags = oldflags;
    forceshaders = wasforcing;
    standardshader = wasstandard;
    delete[] defer;

    if(s->type&SHADER_DEFERRED)
    {
        DELETEA(s->defer);
        s->type = SHADER_INVALID;
    }
}

void fixshaderdetail()
{
    // must null out separately because fixdetailshader can recursively set it
    enumerate(shaders, Shader, s, { if(!s.forced) s.detailshader = NULL; });
    enumerate(shaders, Shader, s, { if(s.forced) s.fixdetailshader(); }); 
    linkslotshaders();
}

int Shader::uniformlocversion()
{
    static int version = 0;
    if(++version >= 0) return version;
    version = 0;
    enumerate(shaders, Shader, s, { loopvj(s.uniformlocs) s.uniformlocs[j].version = -1; });
    return version;
}

VARF(nativeshaders, 0, 1, 1, fixshaderdetail());
VARFP(shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL, fixshaderdetail());

void Shader::fixdetailshader(bool force, bool recurse)
{
    Shader *alt = this;
    detailshader = NULL;
    do
    {
        Shader *cur = shaderdetail < MAXSHADERDETAIL ? alt->fastshader[shaderdetail] : alt;
        if(cur->type&SHADER_DEFERRED && force) useshader(cur);
        if(!(cur->type&SHADER_INVALID))
        {
            if(cur->type&SHADER_DEFERRED) break;
            detailshader = cur;
            if(cur->native || !nativeshaders) break;
        }
        alt = alt->altshader;
    } while(alt && alt!=this);

    if(recurse && detailshader) loopi(MAXVARIANTROWS) loopvj(detailshader->variants[i]) detailshader->variants[i][j]->fixdetailshader(force, false);
}

Shader *useshaderbyname(const char *name)
{
    Shader *s = shaders.access(name);
    if(!s) return NULL;
    if(!s->detailshader) s->fixdetailshader(); 
    s->forced = true;
    return s;
}

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
   
    if((!hasCM && strstr(ps, "textureCube")) || (!hasTR && strstr(ps, "texture2DRect")))
    {
        curparams.shrink(0);
        return;
    }
 
    defformatstring(info)("shader %s", name);
    renderprogress(loadprogress, info);
    vector<char> vsbuf, psbuf, vsbak, psbak;
#define GENSHADER(cond, body) \
    if(cond) \
    { \
        if(vsbuf.length()) { vsbak.setsize(0); vsbak.put(vs, strlen(vs)+1); vs = vsbak.getbuf(); vsbuf.setsize(0); } \
        if(psbuf.length()) { psbak.setsize(0); psbak.put(ps, strlen(ps)+1); ps = psbak.getbuf(); psbuf.setsize(0); } \
        body; \
        if(vsbuf.length()) vs = vsbuf.getbuf(); \
        if(psbuf.length()) ps = psbuf.getbuf(); \
    }
    GENSHADER(curparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps));
    Shader *s = newshader(*type, name, vs, ps);
    if(s)
    {
    }
    curparams.shrink(0);
}

void variantshader(int *type, char *name, int *row, char *vs, char *ps)
{
    if(*row < 0)
    {
        shader(type, name, vs, ps);
        return;
    }

    Shader *s = lookupshaderbyname(name);
    if(!s) return;

    defformatstring(varname)("<variant:%d,%d>%s", s->variants[*row].length(), *row, name);
    //defformatstring(info)("shader %s", varname);
    //renderprogress(loadprogress, info);
    vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(s->defaultparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps, s));
    Shader *v = newshader(*type, varname, vs, ps, s, *row);
    if(v)
    {
        if(strstr(ps, "#pragma CUBE2_variant") || strstr(vs, "#pragma CUBE2_variant")) gengenericvariant(*s, varname, vs, ps, *row);
    }
}

void setshader(char *name)
{
    curparams.shrink(0);
    Shader *s = shaders.access(name);
    if(!s)
    {
        conoutf(CON_ERROR, "no such shader: %s", name);
    }
    else curshader = s;
}

ShaderParam *findshaderparam(Slot &s, const char *name, int type = -1, int index = -1)
{
    loopv(s.params)
    {
        ShaderParam &param = s.params[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    }
    if(!s.shader->detailshader) return NULL;
    loopv(s.shader->detailshader->defaultparams)
    {
        ShaderParam &param = s.shader->detailshader->defaultparams[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    }
    return NULL;
}

ShaderParam *findshaderparam(VSlot &s, const char *name, int type = -1, int index = -1)
{
    loopv(s.params)
    {
        ShaderParam &param = s.params[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    }
    return findshaderparam(*s.slot, name, type, index);
}

void resetslotshader()
{
    curshader = NULL;
    curparams.shrink(0);
}

void setslotshader(Slot &s)
{
    s.shader = curshader;
    if(!s.shader)
    {
        s.shader = stdworldshader;
        return;
    }
    loopv(curparams) s.params.add(curparams[i]);
}

static void linkslotshaderparams(vector<ShaderParam> &params, Shader *sh, bool load)
{
    if(sh) loopv(params)
    {
        int loc = -1;
        ShaderParam &param = params[i];
        loopv(sh->defaultparams)
        {
            ShaderParam &dparam = sh->defaultparams[i];
            if(param.name ? dparam.name==param.name : dparam.type==param.type && dparam.index==param.index)
            {
                if(memcmp(param.val, dparam.val, sizeof(param.val))) loc = i;
                break;
            }
        }
        param.loc = loc;
    }
    else if(load) loopv(params) params[i].loc = -1;
}

void linkslotshader(Slot &s, bool load)
{
    if(!s.shader) return;

    if(load && !s.shader->detailshader) s.shader->fixdetailshader();

    Shader *sh = s.shader->detailshader;
    linkslotshaderparams(s.params, sh, load);
}

void linkvslotshader(VSlot &s, bool load)
{
    if(!s.slot->shader) return;

    Shader *sh = s.slot->shader->detailshader;
    linkslotshaderparams(s.params, sh, load);

    if(!sh) return;

    if(s.slot->texmask&(1<<TEX_GLOW))
    {
        ShaderParam *cparam = findshaderparam(s, "glowcolor");
        if(cparam) loopk(3) s.glowcolor[k] = clamp(cparam->val[k], 0.0f, 1.0f);
    }
}

void altshader(char *origname, char *altname)
{
    Shader *orig = shaders.access(origname), *alt = shaders.access(altname);
    if(!orig || !alt) return;
    orig->altshader = alt;
    orig->fixdetailshader(false);
}

void fastshader(char *nice, char *fast, int *detail)
{
    Shader *ns = shaders.access(nice), *fs = shaders.access(fast);
    if(!ns || !fs) return;
    loopi(min(*detail+1, MAXSHADERDETAIL)) ns->fastshader[i] = fs;
    ns->fixdetailshader(false);
}

COMMAND(shader, "isss");
COMMAND(variantshader, "isiss");
COMMAND(setshader, "s");
COMMAND(altshader, "ss");
COMMAND(fastshader, "ssi");
COMMAND(defershader, "iss");
ICOMMAND(forceshader, "s", (const char *name), useshaderbyname(name));

void isshaderdefined(char *name)
{
    Shader *s = lookupshaderbyname(name);
    intret(s ? 1 : 0);
}

void isshadernative(char *name)
{
    Shader *s = lookupshaderbyname(name);
    intret(s && s->native ? 1 : 0);
}

COMMAND(isshaderdefined, "s");
COMMAND(isshadernative, "s");

static hashset<const char *> shaderparamnames(256);

const char *getshaderparamname(const char *name)
{
    const char **exists = shaderparamnames.access(name);
    if(exists) return *exists;
    name = newstring(name);
    shaderparamnames[name] = name;
    return name;
}

void addshaderparam(const char *name, int type, int n, float x, float y, float z, float w)
{
    if((type==SHPARAM_VERTEX || type==SHPARAM_PIXEL) && (n<0 || n>=MAXSHADERPARAMS))
    {
        conoutf(CON_ERROR, "shader param index must be 0..%d\n", MAXSHADERPARAMS-1);
        return;
    }
    if(name) name = getshaderparamname(name);
    loopv(curparams)
    {
        ShaderParam &param = curparams[i];
        if(param.type == type && (name ? param.name==name : param.index == n))
        {
            param.val[0] = x;
            param.val[1] = y;
            param.val[2] = z;
            param.val[3] = w;
            return;
        }
    }
    ShaderParam param = {name, type, n, -1, {x, y, z, w}};
    curparams.add(param);
}

ICOMMAND(setvertexparam, "iffff", (int *n, float *x, float *y, float *z, float *w), addshaderparam(NULL, SHPARAM_VERTEX, *n, *x, *y, *z, *w));
ICOMMAND(setpixelparam, "iffff", (int *n, float *x, float *y, float *z, float *w), addshaderparam(NULL, SHPARAM_PIXEL, *n, *x, *y, *z, *w));
ICOMMAND(setuniformparam, "sffff", (char *name, float *x, float *y, float *z, float *w), addshaderparam(name, SHPARAM_UNIFORM, -1, *x, *y, *z, *w));
ICOMMAND(setshaderparam, "sffff", (char *name, float *x, float *y, float *z, float *w), addshaderparam(name, SHPARAM_LOOKUP, -1, *x, *y, *z, *w));
ICOMMAND(defvertexparam, "siffff", (char *name, int *n, float *x, float *y, float *z, float *w), addshaderparam(name[0] ? name : NULL, SHPARAM_VERTEX, *n, *x, *y, *z, *w));
ICOMMAND(defpixelparam, "siffff", (char *name, int *n, float *x, float *y, float *z, float *w), addshaderparam(name[0] ? name : NULL, SHPARAM_PIXEL, *n, *x, *y, *z, *w));
ICOMMAND(defuniformparam, "sffff", (char *name, float *x, float *y, float *z, float *w), addshaderparam(name, SHPARAM_UNIFORM, -1, *x, *y, *z, *w));

#define NUMPOSTFXBINDS 10

struct postfxtex
{
    GLuint id;
    int scale, used;

    postfxtex() : id(0), scale(0), used(-1) {}
};
vector<postfxtex> postfxtexs;
int postfxbinds[NUMPOSTFXBINDS];
GLuint postfxfb = 0;
int postfxw = 0, postfxh = 0;

struct postfxpass
{
    Shader *shader;
    vec4 params;
    uint inputs, freeinputs;
    int outputbind, outputscale;

    postfxpass() : shader(NULL), inputs(1), freeinputs(1), outputbind(0), outputscale(0) {}
};
vector<postfxpass> postfxpasses;

static int allocatepostfxtex(int scale)
{
    loopv(postfxtexs)
    {
        postfxtex &t = postfxtexs[i];
        if(t.scale==scale && t.used < 0) return i; 
    }
    postfxtex &t = postfxtexs.add();
    t.scale = scale;
    glGenTextures(1, &t.id);
    createtexture(t.id, max(screen->w>>scale, 1), max(screen->h>>scale, 1), NULL, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE_ARB);
    return postfxtexs.length()-1;
}

void cleanuppostfx(bool fullclean)
{
    if(fullclean && postfxfb)
    {
        glDeleteFramebuffers_(1, &postfxfb);
        postfxfb = 0;
    }

    loopv(postfxtexs) glDeleteTextures(1, &postfxtexs[i].id);
    postfxtexs.shrink(0);

    postfxw = 0;
    postfxh = 0;
}

void renderpostfx()
{
    if(postfxpasses.empty()) return;

    if(postfxw != screen->w || postfxh != screen->h) 
    {
        cleanuppostfx(false);
        postfxw = screen->w;
        postfxh = screen->h;
    }

    int binds[NUMPOSTFXBINDS];
    loopi(NUMPOSTFXBINDS) binds[i] = -1;
    loopv(postfxtexs) postfxtexs[i].used = -1;

    binds[0] = allocatepostfxtex(0);
    postfxtexs[binds[0]].used = 0;
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[binds[0]].id);
    glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, screen->w, screen->h);

    if(hasFBO && postfxpasses.length() > 1)
    {
        if(!postfxfb) glGenFramebuffers_(1, &postfxfb);
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, postfxfb);
    }

    setenvparamf("millis", SHPARAM_VERTEX, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);

    loopv(postfxpasses)
    {
        postfxpass &p = postfxpasses[i];

        int tex = -1;
        if(!postfxpasses.inrange(i+1))
        {
            if(hasFBO && postfxpasses.length() > 1) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
        }
        else
        {
            tex = allocatepostfxtex(p.outputscale);
            if(hasFBO) glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, postfxtexs[tex].id, 0);
        }

        int w = tex >= 0 ? max(screen->w>>postfxtexs[tex].scale, 1) : screen->w, 
            h = tex >= 0 ? max(screen->h>>postfxtexs[tex].scale, 1) : screen->h;
        glViewport(0, 0, w, h);
        p.shader->set();
        setlocalparamfv("params", SHPARAM_VERTEX, 0, p.params.v);
        setlocalparamfv("params", SHPARAM_PIXEL, 0, p.params.v);
        int tw = w, th = h, tmu = 0;
        loopj(NUMPOSTFXBINDS) if(p.inputs&(1<<j) && binds[j] >= 0)
        {
            if(!tmu)
            {
                tw = max(screen->w>>postfxtexs[binds[j]].scale, 1);
                th = max(screen->h>>postfxtexs[binds[j]].scale, 1);
            }
            else glActiveTexture_(GL_TEXTURE0_ARB + tmu);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[binds[j]].id);
            ++tmu;
        }
        if(tmu) glActiveTexture_(GL_TEXTURE0_ARB);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0,  0);  glVertex2f(-1, -1);
        glTexCoord2f(tw, 0);  glVertex2f( 1, -1);
        glTexCoord2f(0,  th); glVertex2f(-1,  1);
        glTexCoord2f(tw, th); glVertex2f( 1,  1);
        glEnd();

        loopj(NUMPOSTFXBINDS) if(p.freeinputs&(1<<j) && binds[j] >= 0)
        {
            postfxtexs[binds[j]].used = -1;
            binds[j] = -1;
        }
        if(tex >= 0)
        {
            if(binds[p.outputbind] >= 0) postfxtexs[binds[p.outputbind]].used = -1;
            binds[p.outputbind] = tex;
            postfxtexs[tex].used = p.outputbind;
            if(!hasFBO)
            {
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[tex].id);
                glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, w, h);
            }
        }
    }
}

static bool addpostfx(const char *name, int outputbind, int outputscale, uint inputs, uint freeinputs, const vec4 &params)
{
    if(!hasTR || !*name) return false;
    Shader *s = useshaderbyname(name);
    if(!s)
    {
        conoutf(CON_ERROR, "no such postfx shader: %s", name);
        return false;
    }
    postfxpass &p = postfxpasses.add();
    p.shader = s;
    p.outputbind = outputbind;
    p.outputscale = outputscale;
    p.inputs = inputs;
    p.freeinputs = freeinputs;
    p.params = params;
    return true;
}

void clearpostfx()
{
    postfxpasses.shrink(0);
    cleanuppostfx(false);
}

COMMAND(clearpostfx, "");

ICOMMAND(addpostfx, "siisffff", (char *name, int *bind, int *scale, char *inputs, float *x, float *y, float *z, float *w),
{
    int inputmask = inputs[0] ? 0 : 1;
    int freemask = inputs[0] ? 0 : 1;
    bool freeinputs = true;
    for(; *inputs; inputs++) if(isdigit(*inputs)) 
    {
        inputmask |= 1<<(*inputs-'0');
        if(freeinputs) freemask |= 1<<(*inputs-'0');
    }
    else if(*inputs=='+') freeinputs = false;
    else if(*inputs=='-') freeinputs = true;
    inputmask &= (1<<NUMPOSTFXBINDS)-1;
    freemask &= (1<<NUMPOSTFXBINDS)-1;
    addpostfx(name, clamp(*bind, 0, NUMPOSTFXBINDS-1), max(*scale, 0), inputmask, freemask, vec4(*x, *y, *z, *w));
});

ICOMMAND(setpostfx, "sffff", (char *name, float *x, float *y, float *z, float *w),
{
    clearpostfx();
    if(name[0]) addpostfx(name, 0, 0, 1, 1, vec4(*x, *y, *z, *w));
});

void cleanupshaders()
{
    cleanuppostfx(true);

    defaultshader = notextureshader = nocolorshader = foggedshader = foggednotextureshader = NULL;
    enumerate(shaders, Shader, s, s.cleanup());
    Shader::lastshader = NULL;
    glUseProgramObject_(0);
    loopi(RESERVEDSHADERPARAMS + MAXSHADERPARAMS)
    {
        vertexparamstate[i].dirty = ShaderParamState::INVALID;
        pixelparamstate[i].dirty = ShaderParamState::INVALID;
    }
}

void reloadshaders()
{
    identflags &= ~IDF_PERSIST;
    loadshaders();
    identflags |= IDF_PERSIST;

    linkslotshaders();
    enumerate(shaders, Shader, s, 
    {
        if(!s.standard && !(s.type&(SHADER_DEFERRED|SHADER_INVALID)) && !s.variantshader) 
        {
            defformatstring(info)("shader %s", s.name);
            renderprogress(0.0, info);
            if(!s.compile()) s.cleanup(true);
            loopi(MAXVARIANTROWS) loopvj(s.variants[i])
            {
                Shader *v = s.variants[i][j];
                if((v->reusevs && v->reusevs->type&SHADER_INVALID) || 
                   (v->reuseps && v->reuseps->type&SHADER_INVALID) ||
                   !v->compile())
                    v->cleanup(true);
            }
        }
        if(s.forced && !s.detailshader) s.fixdetailshader();
    });
}

void setupblurkernel(int radius, float sigma, float *weights, float *offsets)
{
    if(radius<1 || radius>MAXBLURRADIUS) return;
    sigma *= 2*radius;
    float total = 1.0f/sigma;
    weights[0] = total;
    offsets[0] = 0;
    // rely on bilinear filtering to sample 2 pixels at once
    // transforms a*X + b*Y into (u+v)*[X*u/(u+v) + Y*(1 - u/(u+v))]
    loopi(radius)
    {
        float weight1 = exp(-((2*i)*(2*i)) / (2*sigma*sigma)) / sigma,
              weight2 = exp(-((2*i+1)*(2*i+1)) / (2*sigma*sigma)) / sigma,
              scale = weight1 + weight2,
              offset = 2*i+1 + weight2 / scale;
        weights[i+1] = scale;
        offsets[i+1] = offset;
        total += 2*scale;
    }
    loopi(radius+1) weights[i] /= total;
    for(int i = radius+1; i <= MAXBLURRADIUS; i++) weights[i] = offsets[i] = 0;
}

void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target)
{
    if(radius<1 || radius>MAXBLURRADIUS) return; 
    static Shader *blurshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } },
                  *blurrectshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } };
    Shader *&s = (target == GL_TEXTURE_RECTANGLE_ARB ? blurrectshader : blurshader)[radius-1][pass];
    if(!s)
    {
        defformatstring(name)("blur%c%d%s", 'x'+pass, radius, target == GL_TEXTURE_RECTANGLE_ARB ? "rect" : "");
        s = lookupshaderbyname(name);
    }
    s->set();
    setlocalparamfv("weights", SHPARAM_PIXEL, 0, weights);
    setlocalparamfv("weights2", SHPARAM_PIXEL, 2, &weights[4]);
    setlocalparamf("offsets", SHPARAM_VERTEX, 1,
        pass==0 ? offsets[1]/size : offsets[0]/size,
        pass==1 ? offsets[1]/size : offsets[0]/size,
        (offsets[2] - offsets[1])/size,
        (offsets[3] - offsets[2])/size);
    loopk(4)
    {
        static const char *names[4] = { "offset4", "offset5", "offset6", "offset7" };
        setlocalparamf(names[k], SHPARAM_PIXEL, 3+k,
            pass==0 ? offsets[4+k]/size : offsets[0]/size,
            pass==1 ? offsets[4+k]/size : offsets[0]/size,
            0, 0);
    }
}

