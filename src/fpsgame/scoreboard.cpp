// creation of scoreboard
#include "cube.h"
#include "engine.h"
#include "game.h"

#include "network_system.h"
#include "of_tools.h"

namespace game
{
    void renderscoreboard(g3d_gui &g, bool firstpass)
    {
        const char *mname = getclientmap();
        defformatstring(modemapstr)("%s: %s", "Syntensity", mname[0] ? mname : "[new map]");

        g.text(modemapstr, 0xFFFF80, "server");

        g.pushlist(); // vertical
        g.pushlist(); // horizontal
            g.background(0x808080, 5);

            using namespace lua;
            engine.getg("get_scoreboard_text");
            if (!engine.is<void*>(-1))
            {
                g.text("No scoreboard text defined.", 0xFFFFDD, NULL);
                g.text("Create global function get_scoreboard_text", 0xFFFFDD, NULL);
                g.text("in order to achieve what you need, see docs", 0xFFFFDD, NULL);
                g.text("if something is not clear.", 0xFFFFDD, NULL);
            }
            else
            {
                engine.call(1, 1);
                // we get a table here
                LUA_TABLE_FOREACH(engine, {
                    int lineUniqueId = engine.t_get<int>(1);
                    char *lt = strdup(engine.t_get<const char*>(2));
                    if (lineUniqueId != -1)
                    {
                        CLogicEntity *entity = LogicSystem::getLogicEntity(lineUniqueId);
                        if (entity)
                        {
                            fpsent *p = dynamic_cast<fpsent*>(entity->dynamicEntity);
                            assert(p);

                            if (GETIV(showpj))
                            {
                                if (p->state == CS_LAGGED)
                                    lt = of_tools_vstrcat(lt, "s", "LAG");
                                else
                                    lt = of_tools_vstrcat(lt, "si", " pj: ", p->plag);
                            }
                            if (!GETIV(showpj) && p->state == CS_LAGGED)
                                lt = of_tools_vstrcat(lt, "s", "LAG");
                            else
                                lt = of_tools_vstrcat(lt, "si", " p: ", p->ping);
                        }
                    }
                    g.text (lt, 0xFFFFDD, NULL);
                    OF_FREE(lt);
                });
            }
            engine.pop(1);

        g.poplist();
        g.poplist();

        // Show network stats
        static int laststatus = 0; 
        float seconds = float(totalmillis-laststatus)/1024.0f;
        static const char *netStats = "";
        if (seconds >= 0.5)
        {
            laststatus = totalmillis;
            netStats = NetworkSystem::Cataloger::briefSummary(seconds).c_str();
        }
        g.text(netStats, 0xFFFF80, "server");
    }

    struct scoreboardgui : g3d_callback
    {
        bool showing;
        vec menupos;
        int menustart;

        scoreboardgui() : showing(false) {}

        void show(bool on)
        {
            if(!showing && on)
            {
                menupos = menuinfrontofplayer();
                menustart = starttime();
            }
            showing = on;
        }

        void gui(g3d_gui &g, bool firstpass)
        {
            g.start(menustart, 0.03f, NULL, false);
            renderscoreboard(g, firstpass);
            g.end();
        }

        void render()
        {
            if(showing) g3d_addgui(this, menupos, (GETIV(scoreboard2d) ? GUI_FORCE_2D : GUI_2D | GUI_FOLLOW) | GUI_BOTTOM);
        }

    } scoreboard;

    void g3d_gamemenus()
    {
        scoreboard.render();
    }
}

// CubeCreate: temporary for variable exports >.>
void scorebshow(bool on)
{
    game::scoreboard.show(on);
}
