/*
    This file is part of the HeavenMS MapleStory Server
    Copyleft (L) 2016 - 2019 RonanLana

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation version 3 as published by
    the Free Software Foundation. You may not use, modify or distribute
    this program under any other version of the GNU Affero General Public
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
package client;

import config.YamlConfig;
import constants.game.GameConstants;
import server.maps.AbstractAnimatedMapObject;
import server.maps.MapleMap;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * @author RonanLana
 */
public abstract class AbstractCharacterObject extends AbstractAnimatedMapObject {
    protected MapleMap map;
    protected int str, dex, luk, int_, hp, maxhp, mp, maxmp;
    protected int hpMpApUsed, remainingAp;
    protected int[] remainingSp = new int[10];
    protected transient int clientmaxhp, clientmaxmp, localmaxhp = 50, localmaxmp = 5;
    protected float transienthp = Float.NEGATIVE_INFINITY, transientmp = Float.NEGATIVE_INFINITY;

    private AbstractCharacterListener listener = null;
    protected Map<Stat, Integer> statUpdates = new HashMap<>();

    protected final Lock effLock = new ReentrantLock(true);
    protected final Lock statRlock;
    protected final Lock statWlock;

    protected AbstractCharacterObject() {
        ReadWriteLock statLock = new ReentrantReadWriteLock(true);
        this.statRlock = statLock.readLock();
        this.statWlock = statLock.writeLock();

        for (int i = 0; i < remainingSp.length; i++) {
            remainingSp[i] = 0;
        }
    }

    protected void setListener(AbstractCharacterListener listener) {
        this.listener = listener;
    }

    public void setMap(MapleMap map) {
        this.map = map;
    }

    public MapleMap getMap() {
        return map;
    }

    public int getStr() {
        statRlock.lock();
        try {
            return str;
        } finally {
            statRlock.unlock();
        }
    }

    public int getDex() {
        statRlock.lock();
        try {
            return dex;
        } finally {
            statRlock.unlock();
        }
    }

    public int getInt() {
        statRlock.lock();
        try {
            return int_;
        } finally {
            statRlock.unlock();
        }
    }

    public int getLuk() {
        statRlock.lock();
        try {
            return luk;
        } finally {
            statRlock.unlock();
        }
    }

    public int getRemainingAp() {
        statRlock.lock();
        try {
            return remainingAp;
        } finally {
            statRlock.unlock();
        }
    }

    protected int getRemainingSp(int jobid) {
        statRlock.lock();
        try {
            return remainingSp[GameConstants.getSkillBook(jobid)];
        } finally {
            statRlock.unlock();
        }
    }

    public int[] getRemainingSps() {
        statRlock.lock();
        try {
            return Arrays.copyOf(remainingSp, remainingSp.length);
        } finally {
            statRlock.unlock();
        }
    }

    public int getHpMpApUsed() {
        statRlock.lock();
        try {
            return hpMpApUsed;
        } finally {
            statRlock.unlock();
        }
    }

    public boolean isAlive() {
        statRlock.lock();
        try {
            return hp > 0;
        } finally {
            statRlock.unlock();
        }
    }

    public int getHp() {
        statRlock.lock();
        try {
            return hp;
        } finally {
            statRlock.unlock();
        }
    }

    public int getMp() {
        statRlock.lock();
        try {
            return mp;
        } finally {
            statRlock.unlock();
        }
    }

    public int getMaxHp() {
        statRlock.lock();
        try {
            return maxhp;
        } finally {
            statRlock.unlock();
        }
    }

    public int getMaxMp() {
        statRlock.lock();
        try {
            return maxmp;
        } finally {
            statRlock.unlock();
        }
    }

    public int getClientMaxHp() {
        return clientmaxhp;
    }

    public int getClientMaxMp() {
        return clientmaxmp;
    }

    public int getCurrentMaxHp() {
        return localmaxhp;
    }

    public int getCurrentMaxMp() {
        return localmaxmp;
    }

    private void setHpMpApUsed(int mpApUsed) {
        this.hpMpApUsed = mpApUsed;
    }

    private void dispatchHpChanged(final int oldHp) {
        listener.onHpChanged(oldHp);
    }

    private void dispatchHpmpPoolUpdated() {
        listener.onHpmpPoolUpdate();
    }

    private void dispatchStatUpdated() {
        listener.onStatUpdate();
    }

    private void dispatchStatPoolUpdateAnnounced() {
        listener.onAnnounceStatPoolUpdate();
    }

    protected void setHp(int newHp) {
        int oldHp = hp;

        int thp = newHp;
        if (thp < 0) {
            thp = 0;
        } else if (thp > localmaxhp) {
            thp = localmaxhp;
        }

        if (this.hp != thp) {
            this.transienthp = Float.NEGATIVE_INFINITY;
        }
        this.hp = thp;

        dispatchHpChanged(oldHp);
    }

    protected void setMp(int newMp) {
        int tmp = newMp;
        if (tmp < 0) {
            tmp = 0;
        } else if (tmp > localmaxmp) {
            tmp = localmaxmp;
        }

        if (this.mp != tmp) {
            this.transientmp = Float.NEGATIVE_INFINITY;
        }
        this.mp = tmp;
    }

    private void setRemainingAp(int remainingAp) {
        this.remainingAp = remainingAp;
    }

    private void setRemainingSp(int remainingSp, int skillbook) {
        this.remainingSp[skillbook] = remainingSp;
    }

    protected void setMaxHp(int hp_) {
        if (this.maxhp < hp_) {
            this.transienthp = Float.NEGATIVE_INFINITY;
        }
        this.maxhp = hp_;
        this.clientmaxhp = hp_; // MapleV83: no 30000 cap
    }

    protected void setMaxMp(int mp_) {
        if (this.maxmp < mp_) {
            this.transientmp = Float.NEGATIVE_INFINITY;
        }
        this.maxmp = mp_;
        this.clientmaxmp = mp_;
    }

    // MapleV83: stats used to travel packed as four 16-bit fields in a long (capped at 32767).
    // A null slot means "unchanged".
    private static Integer[] statPool(Integer v1, Integer v2, Integer v3, Integer v4) {
        return new Integer[]{v1, v2, v3, v4};
    }

    private void changeStatPool(Integer[] hpMpPool, Integer[] strDexIntLuk, int[] newSp, int newAp, boolean silent) {
        effLock.lock();
        statWlock.lock();
        try {
            statUpdates.clear();
            boolean poolUpdate = false;
            boolean statUpdate = false;

            if (hpMpPool != null) {
                Integer newHp = hpMpPool[0];
                Integer newMp = hpMpPool[1];
                Integer newMaxHp = hpMpPool[2];
                Integer newMaxMp = hpMpPool[3];

                if (newMaxHp != null) {
                    if (newMaxHp < 50) {
                        newMaxHp = 50;
                    }

                    poolUpdate = true;
                    setMaxHp(newMaxHp);
                    statUpdates.put(Stat.MAXHP, clientmaxhp);
                    statUpdates.put(Stat.HP, hp);
                }

                if (newHp != null) {
                    setHp(newHp);
                    statUpdates.put(Stat.HP, hp);
                }

                if (newMaxMp != null) {
                    if (newMaxMp < 5) {
                        newMaxMp = 5;
                    }

                    poolUpdate = true;
                    setMaxMp(newMaxMp);
                    statUpdates.put(Stat.MAXMP, clientmaxmp);
                    statUpdates.put(Stat.MP, mp);
                }

                if (newMp != null) {
                    setMp(newMp);
                    statUpdates.put(Stat.MP, mp);
                }
            }

            if (strDexIntLuk != null) {
                Integer newStr = strDexIntLuk[0];
                Integer newDex = strDexIntLuk[1];
                Integer newInt = strDexIntLuk[2];
                Integer newLuk = strDexIntLuk[3];

                if (newStr != null && newStr >= 4) {
                    setStr(newStr);
                    statUpdates.put(Stat.STR, str);
                }

                if (newDex != null && newDex >= 4) {
                    setDex(newDex);
                    statUpdates.put(Stat.DEX, dex);
                }

                if (newInt != null && newInt >= 4) {
                    setInt(newInt);
                    statUpdates.put(Stat.INT, int_);
                }

                if (newLuk != null && newLuk >= 4) {
                    setLuk(newLuk);
                    statUpdates.put(Stat.LUK, luk);
                }

                if (newAp >= 0) {
                    setRemainingAp(newAp);
                    statUpdates.put(Stat.AVAILABLEAP, remainingAp);
                }

                statUpdate = true;
            }

            if (newSp != null) {
                int sp = newSp[0];
                int skillbook = newSp[1];

                setRemainingSp(sp, skillbook);
                statUpdates.put(Stat.AVAILABLESP, remainingSp[skillbook]);
            }

            if (!statUpdates.isEmpty()) {
                if (poolUpdate) {
                    dispatchHpmpPoolUpdated();
                }

                if (statUpdate) {
                    dispatchStatUpdated();
                }

                if (!silent) {
                    dispatchStatPoolUpdateAnnounced();
                }
            }
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void healHpMp() {
        updateHpMp(Integer.MAX_VALUE); // setHp/setMp clamp to the local max
    }

    public void updateHpMp(int x) {
        updateHpMp(x, x);
    }

    public void updateHpMp(int newhp, int newmp) {
        changeHpMp(newhp, newmp, false);
    }

    protected void changeHpMp(int newhp, int newmp, boolean silent) {
        changeHpMpPool(newhp, newmp, null, null, silent);
    }

    private void changeHpMpPool(Integer hp, Integer mp, Integer maxhp, Integer maxmp, boolean silent) {
        changeStatPool(statPool(hp, mp, maxhp, maxmp), null, null, -1, silent);
    }

    public void updateHp(int hp) {
        updateHpMaxHp(hp, null);
    }

    public void updateMaxHp(int maxhp) {
        updateHpMaxHp(null, maxhp);
    }

    public void updateHpMaxHp(int hp, int maxhp) {
        updateHpMaxHp(Integer.valueOf(hp), Integer.valueOf(maxhp));
    }

    private void updateHpMaxHp(Integer hp, Integer maxhp) {
        changeHpMpPool(hp, null, maxhp, null, false);
    }

    public void updateMp(int mp) {
        updateMpMaxMp(mp, null);
    }

    public void updateMaxMp(int maxmp) {
        updateMpMaxMp(null, maxmp);
    }

    public void updateMpMaxMp(int mp, int maxmp) {
        updateMpMaxMp(Integer.valueOf(mp), Integer.valueOf(maxmp));
    }

    private void updateMpMaxMp(Integer mp, Integer maxmp) {
        changeHpMpPool(null, mp, null, maxmp, false);
    }

    public void updateMaxHpMaxMp(int maxhp, int maxmp) {
        changeHpMpPool(null, null, maxhp, maxmp, false);
    }

    protected void enforceMaxHpMp() {
        effLock.lock();
        statWlock.lock();
        try {
            if (mp > localmaxmp || hp > localmaxhp) {
                changeHpMp(hp, mp, false);
            }
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    // MapleV83: stats go to the integer limit, so sums stop there instead of
    // wrapping negative (a full HP bar plus regen used to kill the character).
    protected static int sat(long value) {
        return (int) Math.max(Integer.MIN_VALUE, Math.min(Integer.MAX_VALUE, value));
    }

    public int safeAddHP(int delta) {
        effLock.lock();
        statWlock.lock();
        try {
            if ((long) hp + delta <= 0) {
                delta = -hp + 1;
            }

            addHP(delta);
            return delta;
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void addHP(int delta) {
        effLock.lock();
        statWlock.lock();
        try {
            updateHp(sat((long) hp + delta));
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void addMP(int delta) {
        effLock.lock();
        statWlock.lock();
        try {
            updateMp(sat((long) mp + delta));
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void addMPHP(int hpDelta, int mpDelta) {
        effLock.lock();
        statWlock.lock();
        try {
            updateHpMp(sat((long) hp + hpDelta), sat((long) mp + mpDelta));
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    protected void addMaxMPMaxHP(int hpdelta, int mpdelta, boolean silent) {
        effLock.lock();
        statWlock.lock();
        try {
            changeHpMpPool(null, null, sat((long) maxhp + hpdelta), sat((long) maxmp + mpdelta), silent);
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void addMaxHP(int delta) {
        effLock.lock();
        statWlock.lock();
        try {
            updateMaxHp(sat((long) maxhp + delta));
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void addMaxMP(int delta) {
        effLock.lock();
        statWlock.lock();
        try {
            updateMaxMp(sat((long) maxmp + delta));
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    private void setStr(int str) {
        this.str = str;
    }

    private void setDex(int dex) {
        this.dex = dex;
    }

    private void setInt(int int_) {
        this.int_ = int_;
    }

    private void setLuk(int luk) {
        this.luk = luk;
    }

    public boolean assignStr(int x) {
        return assignStrDexIntLuk(x, null, null, null);
    }

    public boolean assignDex(int x) {
        return assignStrDexIntLuk(null, x, null, null);
    }

    public boolean assignInt(int x) {
        return assignStrDexIntLuk(null, null, x, null);
    }

    public boolean assignLuk(int x) {
        return assignStrDexIntLuk(null, null, null, x);
    }

    public boolean assignHP(int deltaHP, int deltaAp) {
        effLock.lock();
        statWlock.lock();
        try {
            if (remainingAp - deltaAp < 0 || (long) hpMpApUsed + deltaAp < 0 || (long) maxhp + deltaHP > Integer.MAX_VALUE) {
                return false;
            }

            Integer[] hpMpPool = statPool(null, null, maxhp + deltaHP, maxmp);
            Integer[] strDexIntLuk = statPool(str, dex, int_, luk);

            changeStatPool(hpMpPool, strDexIntLuk, null, remainingAp - deltaAp, false);
            setHpMpApUsed(sat((long) hpMpApUsed + deltaAp));
            return true;
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public boolean assignMP(int deltaMP, int deltaAp) {
        effLock.lock();
        statWlock.lock();
        try {
            if (remainingAp - deltaAp < 0 || (long) hpMpApUsed + deltaAp < 0 || (long) maxmp + deltaMP > Integer.MAX_VALUE) {
                return false;
            }

            Integer[] hpMpPool = statPool(null, null, maxhp, maxmp + deltaMP);
            Integer[] strDexIntLuk = statPool(str, dex, int_, luk);

            changeStatPool(hpMpPool, strDexIntLuk, null, remainingAp - deltaAp, false);
            setHpMpApUsed(sat((long) hpMpApUsed + deltaAp));
            return true;
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    private static int apAssigned(Integer x) {
        return x != null ? x : 0;
    }

    public boolean assignStrDexIntLuk(int deltaStr, int deltaDex, int deltaInt, int deltaLuk) {
        return assignStrDexIntLuk(Integer.valueOf(deltaStr), Integer.valueOf(deltaDex), Integer.valueOf(deltaInt), Integer.valueOf(deltaLuk));
    }

    private boolean assignStrDexIntLuk(Integer deltaStr, Integer deltaDex, Integer deltaInt, Integer deltaLuk) {
        effLock.lock();
        statWlock.lock();
        try {
            int apUsed = apAssigned(deltaStr) + apAssigned(deltaDex) + apAssigned(deltaInt) + apAssigned(deltaLuk);
            if (apUsed > remainingAp) {
                return false;
            }

            long newStr = str, newDex = dex, newInt = int_, newLuk = luk;
            if (deltaStr != null) {
                newStr += deltaStr;   // thanks Rohenn for noticing an NPE case after "null" started being used
            }
            if (deltaDex != null) {
                newDex += deltaDex;
            }
            if (deltaInt != null) {
                newInt += deltaInt;
            }
            if (deltaLuk != null) {
                newLuk += deltaLuk;
            }

            if (newStr < 4 || newStr > YamlConfig.config.server.MAX_AP) {
                return false;
            }

            if (newDex < 4 || newDex > YamlConfig.config.server.MAX_AP) {
                return false;
            }

            if (newInt < 4 || newInt > YamlConfig.config.server.MAX_AP) {
                return false;
            }

            if (newLuk < 4 || newLuk > YamlConfig.config.server.MAX_AP) {
                return false;
            }

            int newAp = remainingAp - apUsed;
            updateStrDexIntLuk((int) newStr, (int) newDex, (int) newInt, (int) newLuk, newAp);
            return true;
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void updateStrDexIntLuk(int x) {
        updateStrDexIntLuk(x, x, x, x, -1);
    }

    public void changeRemainingAp(int x, boolean silent) {
        effLock.lock();
        statWlock.lock();
        try {
            changeStrDexIntLuk(str, dex, int_, luk, x, silent);
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    public void gainAp(int deltaAp, boolean silent) {
        effLock.lock();
        statWlock.lock();
        try {
            changeRemainingAp(Math.max(0, sat((long) remainingAp + deltaAp)), silent);
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    protected void updateStrDexIntLuk(int str, int dex, int int_, int luk, int remainingAp) {
        changeStrDexIntLuk(str, dex, int_, luk, remainingAp, false);
    }

    private void changeStrDexIntLuk(Integer str, Integer dex, Integer int_, Integer luk, int remainingAp, boolean silent) {
        changeStatPool(null, statPool(str, dex, int_, luk), null, remainingAp, silent);
    }

    private void changeStrDexIntLukSp(Integer str, Integer dex, Integer int_, Integer luk, int remainingAp, int remainingSp, int skillbook, boolean silent) {
        changeStatPool(null, statPool(str, dex, int_, luk), new int[]{remainingSp, skillbook}, remainingAp, silent);
    }

    protected void updateStrDexIntLukSp(int str, int dex, int int_, int luk, int remainingAp, int remainingSp, int skillbook) {
        changeStrDexIntLukSp(str, dex, int_, luk, remainingAp, remainingSp, skillbook, false);
    }

    protected void setRemainingSp(int[] sps) {
        effLock.lock();
        statWlock.lock();
        try {
            System.arraycopy(sps, 0, remainingSp, 0, sps.length);
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }

    protected void updateRemainingSp(int remainingSp, int skillbook) {
        changeRemainingSp(remainingSp, skillbook, false);
    }

    protected void changeRemainingSp(int remainingSp, int skillbook, boolean silent) {
        changeStatPool(null, null, new int[]{remainingSp, skillbook}, Short.MIN_VALUE, silent);
    }

    public void gainSp(int deltaSp, int skillbook, boolean silent) {
        effLock.lock();
        statWlock.lock();
        try {
            changeRemainingSp(Math.max(0, sat((long) remainingSp[skillbook] + deltaSp)), skillbook, silent);
        } finally {
            statWlock.unlock();
            effLock.unlock();
        }
    }
}
