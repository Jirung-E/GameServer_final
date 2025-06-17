myid = 99999;

target_id = nil;
attack_time = 0;

spawn_x = 0;
spawn_y = 0;


-- API functions
function setUid(id)
    myid = id;
end

-- API functions
function setSpawnPosition(x, y)
    spawn_x = x;
    spawn_y = y;
end

-- API functions
function respawn()
    API_setPosition(myid, spawn_x, spawn_y);
    API_reset(my_id);
    target_id = nil;
    attack_time = 0;
end


function getRandomDirection()
    local dirs = {
        {x = 1, y = 0},
        {x = -1, y = 0},
        {x = 0, y = 1},
        {x = 0, y = -1}
    }
    return dirs[math.random(1, #dirs)]
end


function attack(p_id)
    API_attack(my_id, p_id);
    API_sendMessage(myid, p_id, "ATTACK");
end


function attackNears(x, y, near_players, time)
    for _, p_id in ipairs(near_players) do
        local player_x = API_getX(p_id);
        local player_y = API_getY(p_id);
        -- ���ݹ���: 1 (1ĭ + �ֺ� 8ĭ ����)
        local dist_x = math.abs(player_x - x);
        local dist_y = math.abs(player_y - y);
        if(dist_x <= 1 and dist_y <= 1) then
            attack(p_id);
            attack_time = time;
        end
    end
end


function distanceSqTo(x, y, p_id)
    local player_x = API_getX(p_id);
    local player_y = API_getY(p_id);
    return (math.pow(player_x - x, 2) + math.pow(player_y - y, 2));
end


function findTarget(x, y, near_players)
    local nearest_distance_sq = math.huge;

    for _, p_id in ipairs(near_players) do
        local distance_sq = distanceSqTo(x, y, p_id);
        -- �����Ÿ�: 5
        if(distance_sq <= 25 and distance_sq < nearest_distance_sq) then
            target_id = p_id;
            nearest_distance_sq = distance_sq;
        end
    end

    return nearest_distance_sq;
end


-- API functions
-- time: ms
function aiUpdate(x, y, near_players, time)
    -- Ÿ���� ���� �þ� �ȿ� �ִ��� Ȯ���Ѵ�.
    if(target_id ~= nil) then
        local distance_sq = distanceSqTo(x, y, target_id);
        -- �þ�: 6
        if(distance_sq > 36) then
            target_id = nil;
        end
    end

    -- Ÿ�� �÷��̾ ������ Ÿ���� ã�´�.
    if(target_id == nil) then
        local distance = findTarget(x, y, near_players);
    end

    -- ���� ���� �ȿ� �ִ� �÷��̾ ������ �����Ѵ�.
    if(target_id ~= nil and time - attack_time >= 1000) then
        attackNears(x, y, near_players, time);
    end

    -- Ÿ�� �÷��̾ ������ �����ϰ� �����δ�.
    if(target_id == nil) then
        move_dir = getRandomDirection();
        API_move(myid, move_dir.x, move_dir.y);
        return;
    -- Ÿ�� �÷��̾ ������ �� �÷��̾ ���� �����δ�.
    else
        local target_x = API_getX(target_id);
        local target_y = API_getY(target_id);
        local dist_x = math.abs(target_x - x);
        local dist_y = math.abs(target_y - y);
        if(dist_x <= 1 and dist_y <= 1) then
            -- Ÿ���� ���ݹ����� ������ �̵����� ����
        else 
            -- Ÿ���� ���� �̵�
            local distance_sq = (math.pow(dist_x, 2) + math.pow(dist_y, 2));
            -- �þ�: 6
            if(distance_sq > 36) then
                target_id = nil;
            else
                API_moveTo(myid, target_x, target_y);
                API_sendMessage(myid, target_id, "CHASE");
            end
        end
    end
end


-- API functions
function event_playerMove(player_x, player_y, p_id, time)
    if(target_id == nil) then
        local x = API_getX(myid);
        local y = API_getY(myid);
        local distance_sq = distanceSqTo(x, y, p_id);
        -- �����Ÿ�: 5
        if(distance_sq <= 25) then
            target_id = p_id;
        end
    end
end

-- API functions
function event_playerAttack(p_id, time)
    local hp = API_getHP(myid);
    if(hp <= 0) then
        respawn();
        target_id = nil;
    else
        API_sendMessage(myid, p_id, "HIT");
    end
end